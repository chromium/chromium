// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/test/print_mock_render_thread.h"

#include <stddef.h>

#include "base/values.h"
#include "build/build_config.h"
#include "components/printing/test/mock_printer.h"
#include "ipc/ipc_sync_message.h"
#include "printing/buildflags/buildflags.h"
#include "printing/page_range.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/common/print_messages.h"
#endif

PrintMockRenderThread::PrintMockRenderThread()
#if BUILDFLAG(ENABLE_PRINTING)
    : printer_(std::make_unique<MockPrinter>())
#endif
{
}

PrintMockRenderThread::~PrintMockRenderThread() {
}

scoped_refptr<base::SingleThreadTaskRunner>
PrintMockRenderThread::GetIOTaskRunner() {
  return io_task_runner_;
}

void PrintMockRenderThread::set_io_task_runner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  io_task_runner_ = task_runner;
}

bool PrintMockRenderThread::OnMessageReceived(const IPC::Message& msg) {
  if (content::MockRenderThread::OnMessageReceived(msg))
    return true;

  // Some messages we do special handling.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintMockRenderThread, msg)
#if BUILDFLAG(ENABLE_PRINTING)
    IPC_MESSAGE_HANDLER(PrintHostMsg_GetDefaultPrintSettings,
                        OnGetDefaultPrintSettings)
    IPC_MESSAGE_HANDLER(PrintHostMsg_ScriptedPrint, OnScriptedPrint)
    IPC_MESSAGE_HANDLER(PrintHostMsg_UpdatePrintSettings, OnUpdatePrintSettings)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetPrintedPagesCount,
                        OnDidGetPrintedPagesCount)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_DidPrintDocument,
                                    OnDidPrintDocument)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidStartPreview, OnDidStartPreview)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPreviewPage, OnDidPreviewPage)
    IPC_MESSAGE_HANDLER(PrintHostMsg_CheckForCancel, OnCheckForCancel)
#endif
#endif  // BUILDFLAG(ENABLE_PRINTING)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if BUILDFLAG(ENABLE_PRINTING)

void PrintMockRenderThread::OnGetDefaultPrintSettings(
    PrintMsg_Print_Params* params) {
  printer_->GetDefaultPrintSettings(params);
}

void PrintMockRenderThread::OnScriptedPrint(
    const PrintHostMsg_ScriptedPrint_Params& params,
    PrintMsg_PrintPages_Params* settings) {
  if (print_dialog_user_response_) {
    printer_->ScriptedPrint(params.cookie, params.expected_pages_count,
                            params.has_selection, settings);
  }
}

void PrintMockRenderThread::OnDidGetPrintedPagesCount(int cookie,
                                                      int number_pages) {
  printer_->SetPrintedPagesCount(cookie, number_pages);
}

void PrintMockRenderThread::OnDidPrintDocument(
    const PrintHostMsg_DidPrintDocument_Params& params,
    IPC::Message* reply_msg) {
  printer_->PrintPage(params);
  PrintHostMsg_DidPrintDocument::WriteReplyParams(reply_msg, true);
  Send(reply_msg);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintMockRenderThread::OnDidStartPreview(
    const PrintHostMsg_DidStartPreview_Params& params,
    const PrintHostMsg_PreviewIds& ids) {
  print_preview_pages_remaining_ = params.page_count;
}

void PrintMockRenderThread::OnDidPreviewPage(
    const PrintHostMsg_DidPreviewPage_Params& params,
    const PrintHostMsg_PreviewIds& ids) {
  DCHECK_GE(params.page_number, printing::FIRST_PAGE_INDEX);
  print_preview_pages_remaining_--;
  print_preview_pages_.emplace_back(
      params.page_number, params.content.metafile_data_region.GetSize());
}

void PrintMockRenderThread::OnCheckForCancel(const PrintHostMsg_PreviewIds& ids,
                                             bool* cancel) {
  *cancel =
      (print_preview_pages_remaining_ == print_preview_cancel_page_number_);
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintMockRenderThread::OnUpdatePrintSettings(
    int document_cookie,
    const base::DictionaryValue& job_settings,
    PrintMsg_PrintPages_Params* params,
    bool* canceled) {
  if (canceled)
    *canceled = false;
  // Check and make sure the required settings are all there.
  // We don't actually care about the values.
  base::Optional<int> margins_type =
      job_settings.FindIntKey(printing::kSettingMarginsType);
  if (!margins_type.has_value() ||
      !job_settings.FindBoolKey(printing::kSettingLandscape) ||
      !job_settings.FindBoolKey(printing::kSettingCollate) ||
      !job_settings.FindIntKey(printing::kSettingColor) ||
      !job_settings.FindIntKey(printing::kSettingPrinterType) ||
      !job_settings.FindBoolKey(printing::kIsFirstRequest) ||
      !job_settings.FindStringKey(printing::kSettingDeviceName) ||
      !job_settings.FindIntKey(printing::kSettingDuplexMode) ||
      !job_settings.FindIntKey(printing::kSettingCopies) ||
      !job_settings.FindIntKey(printing::kPreviewUIID) ||
      !job_settings.FindIntKey(printing::kPreviewRequestID)) {
    return;
  }

  // Just return the default settings.
  const base::Value* page_range =
      job_settings.FindListKey(printing::kSettingPageRange);
  printing::PageRanges new_ranges;
  if (page_range) {
    for (const base::Value& dict : page_range->GetList()) {
      if (!dict.is_dict())
        continue;

      base::Optional<int> range_from =
          dict.FindIntKey(printing::kSettingPageRangeFrom);
      base::Optional<int> range_to =
          dict.FindIntKey(printing::kSettingPageRangeTo);
      if (!range_from || !range_to)
        continue;

      // Page numbers are 1-based in the dictionary.
      // Page numbers are 0-based for the printing context.
      printing::PageRange range;
      range.from = range_from.value() - 1;
      range.to = range_to.value() - 1;
      new_ranges.push_back(range);
    }
  }

  // Get media size
  const base::Value* media_size_value =
      job_settings.FindDictKey(printing::kSettingMediaSize);
  gfx::Size page_size;
  if (media_size_value) {
    base::Optional<int> width_microns =
        media_size_value->FindIntKey(printing::kSettingMediaSizeWidthMicrons);
    base::Optional<int> height_microns =
        media_size_value->FindIntKey(printing::kSettingMediaSizeHeightMicrons);

    if (width_microns && height_microns) {
      float device_microns_per_unit =
          static_cast<float>(printing::kMicronsPerInch) /
          printing::kDefaultPdfDpi;
      page_size = gfx::Size(width_microns.value() / device_microns_per_unit,
                            height_microns.value() / device_microns_per_unit);
    }
  }

  // Get scaling
  base::Optional<int> setting_scale_factor =
      job_settings.FindIntKey(printing::kSettingScaleFactor);
  int scale_factor = setting_scale_factor.value_or(100);

  std::vector<int> pages(printing::PageRange::GetPages(new_ranges));
  printer_->UpdateSettings(document_cookie, params, pages, margins_type.value(),
                           page_size, scale_factor);
  base::Optional<bool> selection_only =
      job_settings.FindBoolKey(printing::kSettingShouldPrintSelectionOnly);
  base::Optional<bool> should_print_backgrounds =
      job_settings.FindBoolKey(printing::kSettingShouldPrintBackgrounds);
  params->params.selection_only = selection_only.value();
  params->params.should_print_backgrounds = should_print_backgrounds.value();
}

MockPrinter* PrintMockRenderThread::printer() {
  return printer_.get();
}

void PrintMockRenderThread::set_print_dialog_user_response(bool response) {
  print_dialog_user_response_ = response;
}

void PrintMockRenderThread::set_print_preview_cancel_page_number(int page) {
  print_preview_cancel_page_number_ = page;
}

int PrintMockRenderThread::print_preview_pages_remaining() const {
  return print_preview_pages_remaining_;
}

const std::vector<std::pair<int, uint32_t>>&
PrintMockRenderThread::print_preview_pages() const {
  return print_preview_pages_;
}
#endif  // BUILDFLAG(ENABLE_PRINTING)
