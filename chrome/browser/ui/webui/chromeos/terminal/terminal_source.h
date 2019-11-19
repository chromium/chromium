// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_TERMINAL_TERMINAL_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_TERMINAL_TERMINAL_SOURCE_H_

#include <string>

#include "base/macros.h"
#include "build/buildflag.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"

class TerminalSource : public content::URLDataSource {
 public:
  TerminalSource() = default;
  ~TerminalSource() override = default;

 private:
  std::string GetSource() override;
#if !BUILDFLAG(OPTIMIZE_WEBUI)
  bool AllowCaching() override;
#endif

  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;

  std::string GetMimeType(const std::string& path) override;

  bool ShouldServeMimeTypeAsContentTypeHeader() override;

  DISALLOW_COPY_AND_ASSIGN(TerminalSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_TERMINAL_TERMINAL_SOURCE_H_
