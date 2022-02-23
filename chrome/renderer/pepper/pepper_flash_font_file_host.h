// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PEPPER_FLASH_FONT_FILE_HOST_H_
#define CHROME_RENDERER_PEPPER_PEPPER_FLASH_FONT_FILE_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include "build/build_config.h"
#include "ppapi/c/private/pp_private_font_charset.h"
#include "ppapi/host/resource_host.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OPENBSD)
#include "base/files/file.h"
#elif BUILDFLAG(IS_WIN)
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"
#endif

namespace content {
class RendererPpapiHost;
}

namespace ppapi {
namespace proxy {
struct SerializedFontDescription;
}
}

class PepperFlashFontFileHost : public ppapi::host::ResourceHost {
 public:
  PepperFlashFontFileHost(
      content::RendererPpapiHost* host,
      PP_Instance instance,
      PP_Resource resource,
      const ppapi::proxy::SerializedFontDescription& description,
      PP_PrivateFontCharset charset);

  PepperFlashFontFileHost(const PepperFlashFontFileHost&) = delete;
  PepperFlashFontFileHost& operator=(const PepperFlashFontFileHost&) = delete;

  ~PepperFlashFontFileHost() override;

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

 private:
  int32_t OnGetFontTable(ppapi::host::HostMessageContext* context,
                         uint32_t table);
  bool GetFontData(uint32_t table, void* buffer, size_t* length);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::File font_file_;
#elif BUILDFLAG(IS_WIN)
  sk_sp<SkTypeface> typeface_;
#endif
};

#endif  // CHROME_RENDERER_PEPPER_PEPPER_FLASH_FONT_FILE_HOST_H_
