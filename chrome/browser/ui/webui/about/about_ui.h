// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ABOUT_ABOUT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ABOUT_ABOUT_UI_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class AboutUI;
class Profile;

namespace content {
class WebUI;
}  // namespace content

// AboutUI is used by multiple chrome:// pages.

class AboutUIConfigBase : public content::DefaultWebUIConfig<AboutUI> {
 public:
  explicit AboutUIConfigBase(std::string_view host);
};

// chrome://chrome-urls. Note that HandleChromeAboutAndChromeSyncRewrite()
// rewrites chrome://about -> chrome://chrome-urls.
class ChromeURLsUIConfig : public AboutUIConfigBase {
 public:
  ChromeURLsUIConfig();
};

// chrome://credits.
class CreditsUIConfig : public AboutUIConfigBase {
 public:
  CreditsUIConfig();
};

#if !BUILDFLAG(IS_ANDROID)
// chrome://terms
class TermsUIConfig : public AboutUIConfigBase {
 public:
  TermsUIConfig();
};
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OPENBSD)
// chrome://linux-proxy-config
class LinuxProxyConfigUI : public AboutUIConfigBase {
 public:
  LinuxProxyConfigUI();
};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// chrome://os-credits
class OSCreditsUI : public AboutUIConfigBase {
 public:
  OSCreditsUI();
};

// chrome://borealis-credits
class BorealisCreditsUI : public AboutUIConfigBase {
 public:
  BorealisCreditsUI();
};

// chrome://crostini-credits
class CrostiniCreditsUI : public AboutUIConfigBase {
 public:
  CrostiniCreditsUI();
};
#endif

// We expose this class because the OOBE flow may need to explicitly add the
// chrome://terms source outside of the normal flow.
class AboutUIHTMLSource : public content::URLDataSource {
 public:
  // Construct a data source for the specified |source_name|.
  AboutUIHTMLSource(const std::string& source_name, Profile* profile);

  AboutUIHTMLSource(const AboutUIHTMLSource&) = delete;
  AboutUIHTMLSource& operator=(const AboutUIHTMLSource&) = delete;

  ~AboutUIHTMLSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  std::string GetAccessControlAllowOriginForOrigin(
      const std::string& origin) override;

  // Send the response data.
  void FinishDataRequest(const std::string& html,
                         content::URLDataSource::GotDataCallback callback);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetOSCreditsPrefixForTesting(const base::FilePath& prefix) {
    os_credits_prefix_ = prefix;
  }
#endif

  Profile* profile() { return profile_; }

 private:
  std::string source_name_;
  raw_ptr<Profile> profile_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath os_credits_prefix_;
#endif
};

class AboutUI : public content::WebUIController {
 public:
  explicit AboutUI(content::WebUI* web_ui, const GURL& url);

  AboutUI(const AboutUI&) = delete;
  AboutUI& operator=(const AboutUI&) = delete;

  ~AboutUI() override = default;

#if BUILDFLAG(IS_CHROMEOS)
  bool OverrideHandleWebUIMessage(const GURL& source_url,
                                  const std::string& message,
                                  const base::Value::List& args) override;
#endif
};

namespace about_ui {

// Helper functions
void AppendHeader(std::string* output, const std::string& unescaped_title);
void AppendBody(std::string *output);
void AppendFooter(std::string *output);

}  // namespace about_ui

#endif  // CHROME_BROWSER_UI_WEBUI_ABOUT_ABOUT_UI_H_
