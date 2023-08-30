// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_CHROME_RENDERER_PEPPER_HOST_FACTORY_H_
#define CHROME_RENDERER_PEPPER_CHROME_RENDERER_PEPPER_HOST_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "ppapi/host/host_factory.h"

namespace content {
class RendererPpapiHost;
}

class ChromeRendererPepperHostFactory : public ppapi::host::HostFactory {
 public:
  explicit ChromeRendererPepperHostFactory(content::RendererPpapiHost* host);

  ChromeRendererPepperHostFactory(const ChromeRendererPepperHostFactory&) =
      delete;
  ChromeRendererPepperHostFactory& operator=(
      const ChromeRendererPepperHostFactory&) = delete;

  ~ChromeRendererPepperHostFactory() override;

  // HostFactory.
  std::unique_ptr<ppapi::host::ResourceHost> CreateResourceHost(
      ppapi::host::PpapiHost* host,
      PP_Resource resource,
      PP_Instance instance,
      const IPC::Message& message) override;

 private:
  // Not owned by this object.
  raw_ptr<content::RendererPpapiHost> host_;
};

#endif  // CHROME_RENDERER_PEPPER_CHROME_RENDERER_PEPPER_HOST_FACTORY_H_
