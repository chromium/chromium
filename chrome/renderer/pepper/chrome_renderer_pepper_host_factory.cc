// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/chrome_renderer_pepper_host_factory.h"

#include "base/check_op.h"
#include "chrome/renderer/pepper/pepper_flash_font_file_host.h"
#include "chrome/renderer/pepper/pepper_flash_fullscreen_host.h"
#include "chrome/renderer/pepper/pepper_uma_host.h"
#include "components/pdf/renderer/pepper_pdf_host.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

using ppapi::host::ResourceHost;

ChromeRendererPepperHostFactory::ChromeRendererPepperHostFactory(
    content::RendererPpapiHost* host)
    : host_(host) {}

ChromeRendererPepperHostFactory::~ChromeRendererPepperHostFactory() {}

std::unique_ptr<ResourceHost>
ChromeRendererPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    PP_Resource resource,
    PP_Instance instance,
    const IPC::Message& message) {
  DCHECK_EQ(host_->GetPpapiHost(), host);

  // Make sure the plugin is giving us a valid instance for this resource.
  if (!host_->IsValidInstance(instance))
    return nullptr;

  if (host_->GetPpapiHost()->permissions().HasPermission(
          ppapi::PERMISSION_FLASH)) {
    switch (message.type()) {
      case PpapiHostMsg_FlashFullscreen_Create::ID: {
        return std::make_unique<PepperFlashFullscreenHost>(host_, instance,
                                                           resource);
      }
    }
  }

  // TODO(raymes): PDF also needs access to the FlashFontFileHost currently.
  // We should either rename PPB_FlashFont_File to PPB_FontFile_Private or get
  // rid of its use in PDF if possible.
  if (host_->GetPpapiHost()->permissions().HasPermission(
          ppapi::PERMISSION_FLASH) ||
      host_->GetPpapiHost()->permissions().HasPermission(
          ppapi::PERMISSION_PDF)) {
    switch (message.type()) {
      case PpapiHostMsg_FlashFontFile_Create::ID: {
        ppapi::proxy::SerializedFontDescription description;
        PP_PrivateFontCharset charset;
        if (ppapi::UnpackMessage<PpapiHostMsg_FlashFontFile_Create>(
                message, &description, &charset)) {
          return std::make_unique<PepperFlashFontFileHost>(
              host_, instance, resource, description, charset);
        }
        break;
      }
    }
  }

  if (host_->GetPpapiHost()->permissions().HasPermission(
          ppapi::PERMISSION_PDF)) {
    switch (message.type()) {
      case PpapiHostMsg_PDF_Create::ID: {
        return std::make_unique<pdf::PepperPDFHost>(host_, instance, resource);
      }
    }
  }

  // Permissions for the following interfaces will be checked at the
  // time of the corresponding instance's method calls.  Currently these
  // interfaces are available only for whitelisted apps which may not have
  // access to the other private interfaces.
  switch (message.type()) {
    case PpapiHostMsg_UMA_Create::ID: {
      return std::make_unique<PepperUMAHost>(host_, instance, resource);
    }
  }

  return nullptr;
}
