// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/chrome_renderer_pepper_host_factory.h"

#include "base/check_op.h"
#include "chrome/renderer/pepper/pepper_uma_host.h"
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
