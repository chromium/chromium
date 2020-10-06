// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/pepper_flash_font_file_host.h"

#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "content/public/renderer/renderer_ppapi_host.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_structs.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_OPENBSD)
#include "components/services/font/public/cpp/font_loader.h"
#include "content/public/common/common_sandbox_support_linux.h"
#elif defined(OS_WIN)
#include "third_party/skia/include/core/SkFontMgr.h"
#endif

PepperFlashFontFileHost::PepperFlashFontFileHost(
    content::RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const ppapi::proxy::SerializedFontDescription& description,
    PP_PrivateFontCharset charset)
    : ResourceHost(host->GetPpapiHost(), instance, resource) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // The global SkFontConfigInterface is configured and initialized with a
  // SkFontconfigInterface compatible font_service::FontLoader in
  // RendererBlinkPlatformImpl (called from RenderThreadImpl::Init) at startup
  // of the mimehandler process, which is a renderer type process. We can reuse
  // it here to call a plugin specific font matching method out of process here.
  // TODO(drott): Find a way to pass this through instead of casting.
  font_service::FontLoader* font_loader_casted =
      reinterpret_cast<font_service::FontLoader*>(
          SkFontConfigInterface::RefGlobal().get());
  font_loader_casted->MatchFontWithFallback(
      description.face,
      description.weight >= PP_BROWSERFONT_TRUSTED_WEIGHT_BOLD,
      description.italic, charset, PP_BROWSERFONT_TRUSTED_FAMILY_DEFAULT,
      &font_file_);
#elif defined(OS_WIN)
  int weight = description.weight;
  if (weight == FW_DONTCARE)
    weight = SkFontStyle::kNormal_Weight;
  SkFontStyle style(weight, SkFontStyle::kNormal_Width,
                    description.italic ? SkFontStyle::kItalic_Slant
                                       : SkFontStyle::kUpright_Slant);
  sk_sp<SkFontMgr> font_mgr(SkFontMgr::RefDefault());

  typeface_ = sk_sp<SkTypeface>(
      font_mgr->matchFamilyStyle(description.face.c_str(), style));
#endif
}

PepperFlashFontFileHost::~PepperFlashFontFileHost() {}

int32_t PepperFlashFontFileHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperFlashFontFileHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFontFile_GetFontTable,
                                      OnGetFontTable)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperFlashFontFileHost::GetFontData(uint32_t table,
                                          void* buffer,
                                          size_t* length) {
  bool result = false;
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  if (font_file_.IsValid()) {
    result = content::GetFontTable(font_file_.GetPlatformFile(), table,
                                   0 /* offset */,
                                   reinterpret_cast<uint8_t*>(buffer), length);
  }
#elif defined(OS_WIN)
  if (typeface_) {
    table = base::ByteSwap(table);
    if (buffer == NULL) {
      *length = typeface_->getTableSize(table);
      if (*length > 0)
        result = true;
    } else {
      size_t new_length = typeface_->getTableData(table, 0, *length, buffer);
      if (new_length == *length)
        result = true;
    }
  }
#endif
  return result;
}

int32_t PepperFlashFontFileHost::OnGetFontTable(
    ppapi::host::HostMessageContext* context,
    uint32_t table) {
  std::string contents;
  int32_t result = PP_ERROR_FAILED;
  size_t length = 0;
  if (GetFontData(table, NULL, &length)) {
    contents.resize(length);
    uint8_t* contents_ptr =
        reinterpret_cast<uint8_t*>(const_cast<char*>(contents.c_str()));
    if (GetFontData(table, contents_ptr, &length)) {
      result = PP_OK;
    } else {
      contents.clear();
    }
  }
  context->reply_msg = PpapiPluginMsg_FlashFontFile_GetFontTableReply(contents);
  return result;
}
