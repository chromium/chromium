// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/cookie_controls_handler.h"
#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/browser/ui/webui/theme_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

namespace {

// Strings sent to the page via jstemplates used to set the direction of the
// HTML document based on locale.
const char kRTLHtmlTextDirection[] = "rtl";
const char kLTRHtmlTextDirection[] = "ltr";

const char* GetHtmlTextDirection(const std::u16string& text) {
  if (base::i18n::IsRTL() && base::i18n::StringContainsStrongRTLChars(text))
    return kRTLHtmlTextDirection;
  return kLTRHtmlTextDirection;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// NewTabUI

NewTabUI::NewTabUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  Profile* profile = GetProfile();

  // The title should be "New Tab" for regular mode and guest mode, while it
  // should be "New Incognito Tab" for incognito mode.
  const int title_resource_id =
      profile->IsOffTheRecord() && !profile->IsGuestSession()
          ? IDS_NEW_INCOGNITO_TAB_TITLE
          : IDS_NEW_TAB_TITLE;
  web_ui->OverrideTitle(l10n_util::GetStringUTF16(title_resource_id));

  if (!profile->IsGuestSession()) {
    web_ui->AddMessageHandler(std::make_unique<ThemeHandler>());
    if (profile->IsOffTheRecord()) {
      web_ui->AddMessageHandler(
          std::make_unique<CookieControlsHandler>(profile));
    }
  }

  // content::URLDataSource assumes the ownership of the html source.
  content::URLDataSource::Add(profile, std::make_unique<NewTabHTMLSource>(
                                           profile->GetOriginalProfile()));
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}

NewTabUI::~NewTabUI() {}

// static
bool NewTabUI::IsNewTab(const GURL& url) {
  return url.DeprecatedGetOriginAsURL() ==
         GURL(chrome::kChromeUINewTabURL).DeprecatedGetOriginAsURL();
}

// static
void NewTabUI::SetUrlTitleAndDirection(base::Value::Dict* dictionary,
                                       const std::u16string& title,
                                       const GURL& gurl) {
  dictionary->Set("url", gurl.spec());

  bool using_url_as_the_title = false;
  std::u16string title_to_set(title);
  if (title_to_set.empty()) {
    using_url_as_the_title = true;
    title_to_set = base::UTF8ToUTF16(gurl.spec());
  }

  // We set the "dir" attribute of the title, so that in RTL locales, a LTR
  // title is rendered left-to-right and truncated from the right. For example,
  // the title of http://msdn.microsoft.com/en-us/default.aspx is "MSDN:
  // Microsoft developer network". In RTL locales, in the [New Tab] page, if
  // the "dir" of this title is not specified, it takes Chrome UI's
  // directionality. So the title will be truncated as "soft developer
  // network". Setting the "dir" attribute as "ltr" renders the truncated title
  // as "MSDN: Microsoft D...". As another example, the title of
  // http://yahoo.com is "Yahoo!". In RTL locales, in the [New Tab] page, the
  // title will be rendered as "!Yahoo" if its "dir" attribute is not set to
  // "ltr".
  std::string direction;
  if (using_url_as_the_title)
    direction = kLTRHtmlTextDirection;
  else
    direction = GetHtmlTextDirection(title);

  dictionary->Set("title", title_to_set);
  dictionary->Set("direction", direction);
}

// static
void NewTabUI::SetFullNameAndDirection(const std::u16string& full_name,
                                       base::Value::Dict* dictionary) {
  dictionary->Set("full_name", full_name);
  dictionary->Set("full_name_direction", GetHtmlTextDirection(full_name));
}

Profile* NewTabUI::GetProfile() const {
  return Profile::FromWebUI(web_ui());
}

///////////////////////////////////////////////////////////////////////////////
// NewTabHTMLSource

NewTabUI::NewTabHTMLSource::NewTabHTMLSource(Profile* profile)
    : profile_(profile) {}

std::string NewTabUI::NewTabHTMLSource::GetSource() {
  return chrome::kChromeUINewTabHost;
}

void NewTabUI::NewTabHTMLSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/40050262): Simplify usages of |path| since |url| is
  // available.
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  if (!path.empty() && path[0] != '#') {
    // A path under new-tab was requested; it's likely a bad relative
    // URL from the new tab page, but in any case it's an error.
    NOTREACHED_IN_MIGRATION()
        << path << " should not have been requested on the NTP";
    std::move(callback).Run(nullptr);
    return;
  }

  // Sometimes the |profile_| is the parent (non-incognito) version of the user
  // so we check the |web_contents| if it is provided.
  content::WebContents* web_contents = wc_getter.Run();
  Profile* profile_for_window_type =
      web_contents
          ? Profile::FromBrowserContext(web_contents->GetBrowserContext())
          : profile_.get();

  NTPResourceCache::WindowType win_type =
      NTPResourceCache::GetWindowType(profile_for_window_type);
  scoped_refptr<base::RefCountedMemory> html_bytes(
      NTPResourceCacheFactory::GetForProfile(profile_)->GetNewTabHTML(
          win_type, wc_getter));

  std::move(callback).Run(html_bytes.get());
}

std::string NewTabUI::NewTabHTMLSource::GetMimeType(const GURL&) {
  return "text/html";
}

bool NewTabUI::NewTabHTMLSource::ShouldReplaceExistingSource() {
  return false;
}

std::string NewTabUI::NewTabHTMLSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  if (directive == network::mojom::CSPDirectiveName::ScriptSrc) {
    // 'unsafe-inline' and google resources are added to script-src.
    return "script-src chrome://resources 'self' 'unsafe-eval' 'unsafe-inline' "
           "*.google.com *.gstatic.com;";
  } else if (directive == network::mojom::CSPDirectiveName::StyleSrc) {
    return "style-src 'self' chrome://resources 'unsafe-inline' "
           "chrome://theme;";
  } else if (directive == network::mojom::CSPDirectiveName::ImgSrc) {
    return "img-src chrome-search://thumb chrome-search://thumb2 "
           "chrome-search://theme chrome://theme data:;";
  } else if (directive == network::mojom::CSPDirectiveName::ChildSrc) {
    return "child-src chrome-search://most-visited;";
  } else if (directive ==
                 network::mojom::CSPDirectiveName::RequireTrustedTypesFor ||
             directive == network::mojom::CSPDirectiveName::TrustedTypes) {
    // TODO(crbug.com/40137143): Trusted Type New Tab Page
    // This removes require-trusted-types-for and trusted-types directives
    // from the CSP header.
    return std::string();
  }

  return content::URLDataSource::GetContentSecurityPolicy(directive);
}

NewTabUI::NewTabHTMLSource::~NewTabHTMLSource() {}
