// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/pdf_to_emf_converter.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/pdf.h"
#include "printing/emf_win.h"
#include "ui/gfx/gdi_util.h"

namespace printing {

namespace {

base::LazyInstance<std::vector<mojo::Remote<mojom::PdfToEmfConverterClient>>>::
    Leaky g_converter_clients = LAZY_INSTANCE_INITIALIZER;

void PreCacheFontCharacters(const LOGFONT* logfont,
                            const wchar_t* text,
                            size_t text_length) {
  if (g_converter_clients.Get().empty()) {
    NOTREACHED()
        << "PreCacheFontCharacters when no converter client is registered.";
    return;
  }

  // We pass the LOGFONT as an array of bytes for simplicity (no typemaps
  // required).
  std::vector<uint8_t> logfont_mojo(sizeof(LOGFONT));
  memcpy(logfont_mojo.data(), logfont, sizeof(LOGFONT));

  g_converter_clients.Get().front()->PreCacheFontCharacters(
      logfont_mojo, base::string16(text, text_length));
}

void OnConvertedClientDisconnected() {
  // We have no direct way of tracking which
  // mojo::Remote<PdfToEmfConverterClient> got disconnected as it is a movable
  // type, short of using a wrapper. Just traverse the list of clients and
  // remove the ones that are not bound.
  base::EraseIf(g_converter_clients.Get(),
                [](const mojo::Remote<mojom::PdfToEmfConverterClient>& client) {
                  return !client.is_bound();
                });
}

void RegisterConverterClient(
    mojo::PendingRemote<mojom::PdfToEmfConverterClient> client_remote) {
  if (!g_converter_clients.IsCreated()) {
    // First time this method is called.
    chrome_pdf::SetPDFEnsureTypefaceCharactersAccessible(
        PreCacheFontCharacters);
  }
  mojo::Remote<mojom::PdfToEmfConverterClient> client(std::move(client_remote));
  client.set_disconnect_handler(base::BindOnce(&OnConvertedClientDisconnected));
  g_converter_clients.Get().push_back(std::move(client));
}

}  // namespace

PdfToEmfConverter::PdfToEmfConverter(
    base::ReadOnlySharedMemoryRegion pdf_region,
    const PdfRenderSettings& pdf_render_settings,
    mojo::PendingRemote<mojom::PdfToEmfConverterClient> client)
    : pdf_render_settings_(pdf_render_settings) {
  RegisterConverterClient(std::move(client));
  SetPrintMode();
  LoadPdf(std::move(pdf_region));
}

PdfToEmfConverter::~PdfToEmfConverter() = default;

void PdfToEmfConverter::SetPrintMode() {
  chrome_pdf::SetPDFUseGDIPrinting(pdf_render_settings_.mode ==
                                   PdfRenderSettings::Mode::GDI_TEXT);
  int printing_mode;
  switch (pdf_render_settings_.mode) {
    case PdfRenderSettings::Mode::TEXTONLY:
      printing_mode = chrome_pdf::PrintingMode::kTextOnly;
      break;
    case PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2:
      printing_mode = chrome_pdf::PrintingMode::kPostScript2;
      break;
    case PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3:
      printing_mode = chrome_pdf::PrintingMode::kPostScript3;
      break;
    default:
      // Not using postscript or text only.
      printing_mode = chrome_pdf::PrintingMode::kEmf;
  }
  chrome_pdf::SetPDFUsePrintMode(printing_mode);
}

void PdfToEmfConverter::LoadPdf(base::ReadOnlySharedMemoryRegion pdf_region) {
  if (!pdf_region.IsValid()) {
    LOG(ERROR) << "Invalid PDF passed to PdfToEmfConverter.";
    return;
  }

  size_t size = pdf_region.GetSize();
  if (size <= 0 || size > std::numeric_limits<int>::max())
    return;

  pdf_mapping_ = pdf_region.Map();
  if (!pdf_mapping_.IsValid())
    return;

  int page_count = 0;
  auto pdf_span = pdf_mapping_.GetMemoryAsSpan<const uint8_t>();
  chrome_pdf::GetPDFDocInfo(pdf_span, &page_count, nullptr);
  total_page_count_ = page_count;
}

base::ReadOnlySharedMemoryRegion PdfToEmfConverter::RenderPdfPageToMetafile(
    int page_number,
    bool postscript,
    float* scale_factor) {
  Emf metafile;
  metafile.Init();

  // We need to scale down DC to fit an entire page into DC available area.
  // Current metafile is based on screen DC and have current screen size.
  // Writing outside of those boundaries will result in the cut-off output.
  // On metafiles (this is the case here), scaling down will still record
  // original coordinates and we'll be able to print in full resolution.
  // Before playback we'll need to counter the scaling up that will happen
  // in the service (print_system_win.cc).
  //
  // The postscript driver does not use the metafile size since it outputs
  // postscript rather than a metafile. Instead it uses the printable area
  // sent to RenderPDFPageToDC to determine the area to render. Therefore,
  // don't scale the DC to match the metafile, and send the printer physical
  // offsets to the driver.
  if (!postscript) {
    *scale_factor = gfx::CalculatePageScale(metafile.context(),
                                            pdf_render_settings_.area.right(),
                                            pdf_render_settings_.area.bottom());
    gfx::ScaleDC(metafile.context(), *scale_factor);
  }

  // The underlying metafile is of type Emf and ignores the arguments passed
  // to StartPage().
  metafile.StartPage(gfx::Size(), gfx::Rect(), 1);
  int offset_x = postscript ? pdf_render_settings_.offsets.x() : 0;
  int offset_y = postscript ? pdf_render_settings_.offsets.y() : 0;

  base::ReadOnlySharedMemoryRegion invalid_emf_region;
  auto pdf_span = pdf_mapping_.GetMemoryAsSpan<const uint8_t>();
  if (!chrome_pdf::RenderPDFPageToDC(
          pdf_span, page_number, metafile.context(),
          pdf_render_settings_.dpi.width(), pdf_render_settings_.dpi.height(),
          pdf_render_settings_.area.x() - offset_x,
          pdf_render_settings_.area.y() - offset_y,
          pdf_render_settings_.area.width(), pdf_render_settings_.area.height(),
          true, false, true, true, pdf_render_settings_.autorotate,
          pdf_render_settings_.use_color)) {
    return invalid_emf_region;
  }
  metafile.FinishPage();
  metafile.FinishDocument();

  const uint32_t size = metafile.GetDataSize();
  base::MappedReadOnlyRegion region_mapping =
      mojo::CreateReadOnlySharedMemoryRegion(size);
  if (!region_mapping.IsValid())
    return invalid_emf_region;

  if (!metafile.GetData(region_mapping.mapping.memory(), size))
    return invalid_emf_region;

  return std::move(region_mapping.region);
}

void PdfToEmfConverter::ConvertPage(uint32_t page_number,
                                    ConvertPageCallback callback) {
  static constexpr float kInvalidScaleFactor = 0;
  base::ReadOnlySharedMemoryRegion invalid_emf_region;
  if (page_number >= total_page_count_) {
    std::move(callback).Run(std::move(invalid_emf_region), kInvalidScaleFactor);
    return;
  }

  float scale_factor = 1.0f;
  bool postscript =
      pdf_render_settings_.mode == PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2 ||
      pdf_render_settings_.mode == PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3;
  base::ReadOnlySharedMemoryRegion emf_region =
      RenderPdfPageToMetafile(page_number, postscript, &scale_factor);
  std::move(callback).Run(std::move(emf_region), scale_factor);
}

}  // namespace printing
