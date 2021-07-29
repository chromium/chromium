// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pepper_pdf_host.h"

#include <memory>

#include "base/lazy_instance.h"
#include "components/pdf/renderer/pdf_accessibility_tree.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/ppapi_gfx_conversion.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "pdf/accessibility_structs.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/shared_impl/ppb_image_data_shared.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "ui/gfx/geometry/point.h"

namespace pdf {

namespace {

// --single-process model may fail in CHECK(!g_print_client) if there exist
// more than two RenderThreads, so here we use TLS for g_print_client.
// See http://crbug.com/457580.
base::LazyInstance<base::ThreadLocalPointer<PepperPDFHost::PrintClient>>::Leaky
    g_print_client_tls = LAZY_INSTANCE_INITIALIZER;

}  // namespace

PepperPDFHost::PepperPDFHost(content::RendererPpapiHost* host,
                             PP_Instance instance,
                             PP_Resource resource)
    : ppapi::host::ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host) {
  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return;

  service->SetListener(receiver_.BindNewPipeAndPassRemote());
}

PepperPDFHost::~PepperPDFHost() {}

// static
bool PepperPDFHost::InvokePrintingForInstance(PP_Instance instance_id) {
  return g_print_client_tls.Pointer()->Get()
             ? g_print_client_tls.Pointer()->Get()->Print(instance_id)
             : false;
}

// static
void PepperPDFHost::SetPrintClient(PepperPDFHost::PrintClient* client) {
  CHECK(!g_print_client_tls.Pointer()->Get())
      << "There should only be a single PrintClient for one RenderThread.";
  g_print_client_tls.Pointer()->Set(client);
}

int32_t PepperPDFHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperPDFHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_DidStartLoading,
                                        OnHostMsgDidStartLoading)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_DidStopLoading,
                                        OnHostMsgDidStopLoading)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_UserMetricsRecordAction,
                                      OnHostMsgUserMetricsRecordAction)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_HasUnsupportedFeature,
                                        OnHostMsgHasUnsupportedFeature)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_Print, OnHostMsgPrint)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_SaveAs,
                                        OnHostMsgSaveAs)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_ShowAlertDialog,
                                      OnHostMsgShowAlertDialog)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_ShowConfirmDialog,
                                      OnHostMsgShowConfirmDialog)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_ShowPromptDialog,
                                      OnHostMsgShowPromptDialog)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_SetSelectedText,
                                      OnHostMsgSetSelectedText)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_SetLinkUnderCursor,
                                      OnHostMsgSetLinkUnderCursor)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_SetContentRestriction,
                                      OnHostMsgSetContentRestriction)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_PDF_SetAccessibilityViewportInfo,
        OnHostMsgSetAccessibilityViewportInfo)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_PDF_SetAccessibilityDocInfo,
        OnHostMsgSetAccessibilityDocInfo)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_PDF_SetAccessibilityPageInfo,
        OnHostMsgSetAccessibilityPageInfo)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_SelectionChanged,
                                      OnHostMsgSelectionChanged)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_SetPluginCanSave,
                                      OnHostMsgSetPluginCanSave)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperPDFHost::OnHostMsgDidStartLoading(
    ppapi::host::HostMessageContext* context) {
  content::RenderFrame* render_frame = GetRenderFrame();
  if (!render_frame)
    return PP_ERROR_FAILED;

  render_frame->PluginDidStartLoading();
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgDidStopLoading(
    ppapi::host::HostMessageContext* context) {
  content::RenderFrame* render_frame = GetRenderFrame();
  if (!render_frame)
    return PP_ERROR_FAILED;

  render_frame->PluginDidStopLoading();
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetContentRestriction(
    ppapi::host::HostMessageContext* context,
    int restrictions) {
  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return PP_ERROR_FAILED;

  service->UpdateContentRestrictions(restrictions);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgUserMetricsRecordAction(
    ppapi::host::HostMessageContext* context,
    const std::string& action) {
  if (action.empty())
    return PP_ERROR_FAILED;
  content::RenderThread::Get()->RecordComputedAction(action);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgHasUnsupportedFeature(
    ppapi::host::HostMessageContext* context) {
  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return PP_ERROR_FAILED;

  service->HasUnsupportedFeature();
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgPrint(
    ppapi::host::HostMessageContext* context) {
  return InvokePrintingForInstance(pp_instance()) ? PP_OK : PP_ERROR_FAILED;
}

int32_t PepperPDFHost::OnHostMsgShowAlertDialog(
    ppapi::host::HostMessageContext* context,
    const std::string& message) {
  blink::WebLocalFrame* frame = GetWebLocalFrame();
  if (!frame)
    return PP_ERROR_FAILED;

  frame->Alert(blink::WebString::FromUTF8(message));
  context->reply_msg = PpapiPluginMsg_PDF_ShowAlertDialogReply();
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgShowConfirmDialog(
    ppapi::host::HostMessageContext* context,
    const std::string& message) {
  blink::WebLocalFrame* frame = GetWebLocalFrame();
  if (!frame)
    return PP_ERROR_FAILED;

  bool bool_result = frame->Confirm(blink::WebString::FromUTF8(message));
  context->reply_msg = PpapiPluginMsg_PDF_ShowConfirmDialogReply(bool_result);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgShowPromptDialog(
    ppapi::host::HostMessageContext* context,
    const std::string& message,
    const std::string& default_answer) {
  blink::WebLocalFrame* frame = GetWebLocalFrame();
  if (!frame)
    return PP_ERROR_FAILED;

  blink::WebString result =
      frame->Prompt(blink::WebString::FromUTF8(message),
                    blink::WebString::FromUTF8(default_answer));
  context->reply_msg = PpapiPluginMsg_PDF_ShowPromptDialogReply(result.Utf8());
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSaveAs(
    ppapi::host::HostMessageContext* context) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;

  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return PP_ERROR_FAILED;

  service->SaveUrlAs(instance->GetPluginURL(),
                     network::mojom::ReferrerPolicy::kDefault);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetSelectedText(
    ppapi::host::HostMessageContext* context,
    const std::u16string& selected_text) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->SetSelectedText(selected_text);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetLinkUnderCursor(
    ppapi::host::HostMessageContext* context,
    const std::string& url) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->SetLinkUnderCursor(url);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetAccessibilityViewportInfo(
    ppapi::host::HostMessageContext* context,
    const PP_PrivateAccessibilityViewportInfo& pp_viewport_info) {
  if (!host_->GetPluginInstance(pp_instance()))
    return PP_ERROR_FAILED;
  CreatePdfAccessibilityTreeIfNeeded();
  chrome_pdf::AccessibilityViewportInfo viewport_info = {
      pp_viewport_info.zoom,
      pp_viewport_info.scale,
      gfx::Point(pp_viewport_info.scroll.x, pp_viewport_info.scroll.y),
      gfx::Point(pp_viewport_info.offset.x, pp_viewport_info.offset.y),
      pp_viewport_info.selection_start_page_index,
      pp_viewport_info.selection_start_char_index,
      pp_viewport_info.selection_end_page_index,
      pp_viewport_info.selection_end_char_index,
      {static_cast<chrome_pdf::FocusObjectType>(
           pp_viewport_info.focus_info.focused_object_type),
       pp_viewport_info.focus_info.focused_object_page_index,
       pp_viewport_info.focus_info.focused_annotation_index_in_page}};
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetAccessibilityDocInfo(
    ppapi::host::HostMessageContext* context,
    const PP_PrivateAccessibilityDocInfo& pp_doc_info) {
  if (!host_->GetPluginInstance(pp_instance()))
    return PP_ERROR_FAILED;
  CreatePdfAccessibilityTreeIfNeeded();
  chrome_pdf::AccessibilityDocInfo doc_info = {
      pp_doc_info.page_count, PP_ToBool(pp_doc_info.text_accessible),
      PP_ToBool(pp_doc_info.text_copyable)};
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info);
  return PP_OK;
}

namespace {

chrome_pdf::AccessibilityTextStyleInfo ToAccessibilityTextStyleInfo(
    const ppapi::PdfAccessibilityTextStyleInfo& pp_style) {
  chrome_pdf::AccessibilityTextStyleInfo style;
  style.font_name = pp_style.font_name;
  style.font_weight = pp_style.font_weight;
  style.render_mode = static_cast<chrome_pdf::AccessibilityTextRenderMode>(
      pp_style.render_mode);
  style.font_size = pp_style.font_size;
  style.fill_color = pp_style.fill_color;
  style.stroke_color = pp_style.stroke_color;
  style.is_italic = pp_style.is_italic;
  style.is_bold = pp_style.is_bold;
  return style;
}

chrome_pdf::AccessibilityPageObjects ToAccessibilityPageObjects(
    const ppapi::PdfAccessibilityPageObjects& pp_page_objects) {
  chrome_pdf::AccessibilityPageObjects page_objects;

  page_objects.links.reserve(pp_page_objects.links.size());
  for (const ppapi::PdfAccessibilityLinkInfo& pp_link : pp_page_objects.links) {
    chrome_pdf::AccessibilityTextRunRangeInfo range_info = {
        pp_link.text_run_index, pp_link.text_run_count};
    page_objects.links.emplace_back(pp_link.url, pp_link.index_in_page,
                                    content::PP_ToGfxRectF(pp_link.bounds),
                                    std::move(range_info));
  }

  page_objects.images.reserve(pp_page_objects.images.size());
  for (const ppapi::PdfAccessibilityImageInfo& pp_image :
       pp_page_objects.images) {
    page_objects.images.emplace_back(pp_image.alt_text, pp_image.text_run_index,
                                     content::PP_ToGfxRectF(pp_image.bounds));
  }

  page_objects.highlights.reserve(pp_page_objects.highlights.size());
  for (const ppapi::PdfAccessibilityHighlightInfo& pp_highlight :
       pp_page_objects.highlights) {
    chrome_pdf::AccessibilityTextRunRangeInfo range_info = {
        pp_highlight.text_run_index, pp_highlight.text_run_count};
    page_objects.highlights.emplace_back(
        pp_highlight.note_text, pp_highlight.index_in_page, pp_highlight.color,
        content::PP_ToGfxRectF(pp_highlight.bounds), std::move(range_info));
  }

  page_objects.form_fields.text_fields.reserve(
      pp_page_objects.form_fields.text_fields.size());
  for (const ppapi::PdfAccessibilityTextFieldInfo& pp_text_field :
       pp_page_objects.form_fields.text_fields) {
    page_objects.form_fields.text_fields.emplace_back(
        pp_text_field.name, pp_text_field.value, pp_text_field.is_read_only,
        pp_text_field.is_required, pp_text_field.is_password,
        pp_text_field.index_in_page, pp_text_field.text_run_index,
        content::PP_ToGfxRectF(pp_text_field.bounds));
  }

  page_objects.form_fields.choice_fields.reserve(
      pp_page_objects.form_fields.choice_fields.size());
  for (const ppapi::PdfAccessibilityChoiceFieldInfo& pp_choice_field :
       pp_page_objects.form_fields.choice_fields) {
    std::vector<chrome_pdf::AccessibilityChoiceFieldOptionInfo> options;
    options.reserve(pp_choice_field.options.size());
    for (const ppapi::PdfAccessibilityChoiceFieldOptionInfo& pp_option :
         pp_choice_field.options) {
      options.push_back({pp_option.name, pp_option.is_selected,
                         content::PP_ToGfxRectF(pp_option.bounds)});
    }

    page_objects.form_fields.choice_fields.emplace_back(
        pp_choice_field.name, std::move(options),
        static_cast<chrome_pdf::ChoiceFieldType>(pp_choice_field.type),
        pp_choice_field.is_read_only, pp_choice_field.is_multi_select,
        pp_choice_field.has_editable_text_box, pp_choice_field.index_in_page,
        pp_choice_field.text_run_index,
        content::PP_ToGfxRectF(pp_choice_field.bounds));
  }

  page_objects.form_fields.buttons.reserve(
      pp_page_objects.form_fields.buttons.size());
  for (const ppapi::PdfAccessibilityButtonInfo& pp_button :
       pp_page_objects.form_fields.buttons) {
    page_objects.form_fields.buttons.emplace_back(
        pp_button.name, pp_button.value,
        static_cast<chrome_pdf::ButtonType>(pp_button.type),
        pp_button.is_read_only, pp_button.is_checked, pp_button.control_count,
        pp_button.control_index, pp_button.index_in_page,
        pp_button.text_run_index, content::PP_ToGfxRectF(pp_button.bounds));
  }

  return page_objects;
}

}  // namespace

int32_t PepperPDFHost::OnHostMsgSetAccessibilityPageInfo(
    ppapi::host::HostMessageContext* context,
    const PP_PrivateAccessibilityPageInfo& pp_page_info,
    const std::vector<ppapi::PdfAccessibilityTextRunInfo>& pp_text_run_infos,
    const std::vector<PP_PrivateAccessibilityCharInfo>& pp_chars,
    const ppapi::PdfAccessibilityPageObjects& pp_page_objects) {
  if (!host_->GetPluginInstance(pp_instance()))
    return PP_ERROR_FAILED;
  CreatePdfAccessibilityTreeIfNeeded();
  chrome_pdf::AccessibilityPageInfo page_info = {
      pp_page_info.page_index, content::PP_ToGfxRect(pp_page_info.bounds),
      pp_page_info.text_run_count, pp_page_info.char_count};
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_run_infos;
  text_run_infos.reserve(pp_text_run_infos.size());
  for (const auto& pp_text_run_info : pp_text_run_infos) {
    text_run_infos.emplace_back(
        pp_text_run_info.len, content::PP_ToGfxRectF(pp_text_run_info.bounds),
        static_cast<chrome_pdf::AccessibilityTextDirection>(
            pp_text_run_info.direction),
        ToAccessibilityTextStyleInfo(pp_text_run_info.style));
  }
  std::vector<chrome_pdf::AccessibilityCharInfo> chars;
  chars.reserve(pp_chars.size());
  for (const auto& pp_char : pp_chars)
    chars.push_back({pp_char.unicode_character, pp_char.char_width});
  chrome_pdf::AccessibilityPageObjects page_objects =
      ToAccessibilityPageObjects(pp_page_objects);
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info, text_run_infos,
                                                    chars, page_objects);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSelectionChanged(
    ppapi::host::HostMessageContext* context,
    const PP_FloatPoint& left,
    int32_t left_height,
    const PP_FloatPoint& right,
    int32_t right_height) {
  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return PP_ERROR_FAILED;

  service->SelectionChanged(gfx::PointF(left.x, left.y), left_height,
                            gfx::PointF(right.x, right.y), right_height);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetPluginCanSave(
    ppapi::host::HostMessageContext* context,
    bool can_save) {
  mojom::PdfService* service = GetRemotePdfService();
  if (!service)
    return PP_ERROR_FAILED;

  service->SetPluginCanSave(can_save);
  return PP_OK;
}

void PepperPDFHost::CreatePdfAccessibilityTreeIfNeeded() {
  if (!pdf_accessibility_tree_) {
    pdf_accessibility_tree_ =
        std::make_unique<PdfAccessibilityTree>(GetRenderFrame(), this);
  }
}

content::RenderFrame* PepperPDFHost::GetRenderFrame() {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  return instance ? instance->GetRenderFrame() : nullptr;
}

mojom::PdfService* PepperPDFHost::GetRemotePdfService() {
  content::RenderFrame* render_frame = GetRenderFrame();
  if (!render_frame)
    return nullptr;

  if (!remote_pdf_service_) {
    render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
        remote_pdf_service_.BindNewEndpointAndPassReceiver());
  }
  return remote_pdf_service_.get();
}

blink::WebLocalFrame* PepperPDFHost::GetWebLocalFrame() {
  if (!host_->GetPluginInstance(pp_instance()))
    return nullptr;

  blink::WebPluginContainer* container =
      host_->GetContainerForInstance(pp_instance());
  if (!container)
    return nullptr;

  return container->GetDocument().GetFrame();
}

void PepperPDFHost::SetCaretPosition(const gfx::PointF& position) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (instance)
    instance->SetCaretPosition(position);
}

void PepperPDFHost::MoveRangeSelectionExtent(const gfx::PointF& extent) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (instance)
    instance->MoveRangeSelectionExtent(extent);
}

void PepperPDFHost::SetSelectionBounds(const gfx::PointF& base,
                                       const gfx::PointF& extent) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (instance)
    instance->SetSelectionBounds(base, extent);
}

namespace {

PP_PdfPageCharacterIndex ToPdfPageCharacterIndex(
    const chrome_pdf::PageCharacterIndex& page_char_index) {
  return {
      .page_index = page_char_index.page_index,
      .char_index = page_char_index.char_index,
  };
}

PP_PdfAccessibilityActionData ToPdfAccessibilityActionData(
    const chrome_pdf::AccessibilityActionData& action_data) {
  return {
      .action = static_cast<PP_PdfAccessibilityAction>(action_data.action),
      .annotation_type = static_cast<PP_PdfAccessibilityAnnotationType>(
          action_data.annotation_type),
      .target_point = content::PP_FromGfxPoint(action_data.target_point),
      .target_rect = content::PP_FromGfxRect(action_data.target_rect),
      .annotation_index = action_data.annotation_index,
      .page_index = action_data.page_index,
      .horizontal_scroll_alignment =
          static_cast<PP_PdfAccessibilityScrollAlignment>(
              action_data.horizontal_scroll_alignment),
      .vertical_scroll_alignment =
          static_cast<PP_PdfAccessibilityScrollAlignment>(
              action_data.vertical_scroll_alignment),
      .selection_start_index =
          ToPdfPageCharacterIndex(action_data.selection_start_index),
      .selection_end_index =
          ToPdfPageCharacterIndex(action_data.selection_end_index),
  };
}

}  // namespace

void PepperPDFHost::HandleAccessibilityAction(
    const chrome_pdf::AccessibilityActionData& action_data) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (instance)
    instance->HandleAccessibilityAction(
        ToPdfAccessibilityActionData(action_data));
}

}  // namespace pdf
