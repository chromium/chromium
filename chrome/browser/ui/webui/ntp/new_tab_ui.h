// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_UI_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"

class GURL;
class Profile;

// The WebUIController used for the incognito and guest mode New Tab page.
class NewTabUI : public content::WebUIController {
 public:
  explicit NewTabUI(content::WebUI* web_ui);

  NewTabUI(const NewTabUI&) = delete;
  NewTabUI& operator=(const NewTabUI&) = delete;

  ~NewTabUI() override;

  // Checks whether the given URL points to an NTP WebUI. Note that this only
  // applies to incognito and guest mode NTPs - you probably want to check
  // search::NavEntryIsInstantNTP too!
  static bool IsNewTab(const GURL& url);

  // TODO(dbeam): why are these static |Set*()| methods on NewTabUI?

  // Adds "url", "title", and "direction" keys on incoming dictionary, setting
  // title as the url as a fallback on empty title.
  static void SetUrlTitleAndDirection(base::Value::Dict* dictionary,
                                      const std::u16string& title,
                                      const GURL& gurl);

  // Adds "full_name" and "full_name_direction" keys on incoming dictionary.
  static void SetFullNameAndDirection(const std::u16string& full_name,
                                      base::Value::Dict* dictionary);

 private:
  class NewTabHTMLSource : public content::URLDataSource {
   public:
    explicit NewTabHTMLSource(Profile* profile);

    NewTabHTMLSource(const NewTabHTMLSource&) = delete;
    NewTabHTMLSource& operator=(const NewTabHTMLSource&) = delete;

    ~NewTabHTMLSource() override;

    // content::URLDataSource implementation.
    std::string GetSource() override;
    void StartDataRequest(
        const GURL& url,
        const content::WebContents::Getter& wc_getter,
        content::URLDataSource::GotDataCallback callback) override;
    std::string GetMimeType(const GURL&) override;
    bool ShouldReplaceExistingSource() override;
    std::string GetContentSecurityPolicy(
        network::mojom::CSPDirectiveName directive) override;

   private:
    // Pointer back to the original profile.
    raw_ptr<Profile, FlakyDanglingUntriaged> profile_;
  };

  void OnShowBookmarkBarChanged();

  Profile* GetProfile() const;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_UI_H_
