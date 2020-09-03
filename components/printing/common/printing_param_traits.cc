// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/printing_param_traits.h"

#include "base/containers/flat_map.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/pickle.h"
#include "components/printing/common/print_messages.h"

namespace IPC {

using printing::mojom::DidPrintContentParamsPtr;
using printing::mojom::PrintParamsPtr;

void ParamTraits<DidPrintContentParamsPtr>::Write(base::Pickle* m,
                                                  const param_type& p) {
  WriteParam(m, p->metafile_data_region);
  WriteParam(m, p->subframe_content_info);
}

bool ParamTraits<DidPrintContentParamsPtr>::Read(const base::Pickle* m,
                                                 base::PickleIterator* iter,
                                                 param_type* p) {
  bool success = true;

  base::ReadOnlySharedMemoryRegion metafile_data_region;
  success &= ReadParam(m, iter, &metafile_data_region);

  base::flat_map<uint32_t, base::UnguessableToken> subframe_content_info;
  success &= ReadParam(m, iter, &subframe_content_info);

  if (success) {
    *p = printing::mojom::DidPrintContentParams::New(
        std::move(metafile_data_region), subframe_content_info);
  }
  return success;
}

void ParamTraits<DidPrintContentParamsPtr>::Log(const param_type& p,
                                                std::string* l) {}

void ParamTraits<PrintParamsPtr>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p->page_size);
  WriteParam(m, p->content_size);
  WriteParam(m, p->printable_area);
  m->WriteInt(p->margin_top);
  m->WriteInt(p->margin_left);
  WriteParam(m, p->page_orientation);
  WriteParam(m, p->dpi);
  m->WriteDouble(p->scale_factor);
  m->WriteInt(p->document_cookie);
  m->WriteBool(p->selection_only);
  m->WriteBool(p->supports_alpha_blend);
  m->WriteInt(p->preview_ui_id);
  m->WriteInt(p->preview_request_id);
  m->WriteBool(p->is_first_request);
  WriteParam(m, p->print_scaling_option);
  m->WriteBool(p->print_to_pdf);
  m->WriteBool(p->display_header_footer);
  m->WriteString16(p->title);
  m->WriteString16(p->url);
  m->WriteString16(p->header_template);
  m->WriteString16(p->footer_template);
  m->WriteBool(p->rasterize_pdf);
  m->WriteBool(p->should_print_backgrounds);
  WriteParam(m, p->printed_doc_type);
  m->WriteBool(p->prefer_css_page_size);
  m->WriteUInt32(p->pages_per_sheet);
}

bool ParamTraits<PrintParamsPtr>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  bool success = true;

  gfx::Size page_size;
  success &= ReadParam(m, iter, &page_size);

  gfx::Size content_size;
  success &= ReadParam(m, iter, &content_size);

  gfx::Rect printable_area;
  success &= ReadParam(m, iter, &printable_area);

  int32_t margin_top;
  success &= iter->ReadInt(&margin_top);

  int32_t margin_left;
  success &= iter->ReadInt(&margin_left);

  printing::mojom::PageOrientation page_orientation;
  success &= ReadParam(m, iter, &page_orientation);

  gfx::Size dpi;
  success &= ReadParam(m, iter, &dpi);

  double scale_factor;
  success &= iter->ReadDouble(&scale_factor);

  int32_t document_cookie;
  success &= iter->ReadInt(&document_cookie);

  bool selection_only;
  success &= iter->ReadBool(&selection_only);

  bool supports_alpha_blend;
  success &= iter->ReadBool(&supports_alpha_blend);

  int32_t preview_ui_id;
  success &= iter->ReadInt(&preview_ui_id);

  int32_t preview_request_id;
  success &= iter->ReadInt(&preview_request_id);

  bool is_first_request;
  success &= iter->ReadBool(&is_first_request);

  printing::mojom::PrintScalingOption print_scaling_option;
  success &= ReadParam(m, iter, &print_scaling_option);

  bool print_to_pdf;
  success &= iter->ReadBool(&print_to_pdf);

  bool display_header_footer;
  success &= iter->ReadBool(&display_header_footer);

  base::string16 title;
  success &= iter->ReadString16(&title);

  base::string16 url;
  success &= iter->ReadString16(&url);

  base::string16 header_template;
  success &= iter->ReadString16(&header_template);

  base::string16 footer_template;
  success &= iter->ReadString16(&footer_template);

  bool rasterize_pdf;
  success &= iter->ReadBool(&rasterize_pdf);

  bool should_print_backgrounds;
  success &= iter->ReadBool(&should_print_backgrounds);

  printing::mojom::SkiaDocumentType printed_doc_type;
  success &= ReadParam(m, iter, &printed_doc_type);

  bool prefer_css_page_size;
  success &= iter->ReadBool(&prefer_css_page_size);

  uint32_t pages_per_sheet;
  success &= iter->ReadUInt32(&pages_per_sheet);

  if (success) {
    *p = printing::mojom::PrintParams::New(
        page_size, content_size, printable_area, margin_top, margin_left,
        page_orientation, dpi, scale_factor, document_cookie, selection_only,
        supports_alpha_blend, preview_ui_id, preview_request_id,
        is_first_request, print_scaling_option, print_to_pdf,
        display_header_footer, title, url, header_template, footer_template,
        rasterize_pdf, should_print_backgrounds, printed_doc_type,
        prefer_css_page_size, pages_per_sheet);
  }
  return success;
}

void ParamTraits<PrintParamsPtr>::Log(const param_type& p, std::string* l) {}

}  // namespace IPC
