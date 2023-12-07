// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_HATS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_HATS_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace extensions {

// Extensions page handler that manages and launches the extension safety
// check HaTS survey.
class ExtensionsHatsHandler : public content::WebUIMessageHandler,
                              public content::WebContentsObserver {
 public:
  explicit ExtensionsHatsHandler(Profile* profile);

  // Not copyable or movable
  ExtensionsHatsHandler(const ExtensionsHatsHandler&) = delete;
  ExtensionsHatsHandler& operator=(const ExtensionsHatsHandler&) = delete;

  ~ExtensionsHatsHandler() override;

 private:
  friend class ExtensionsHatsHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(ExtensionsHatsHandlerTest,
                           ExtensionsPageInteractions);

  // Handlers for JS hooks from the webui page.
  void HandleExtensionsSafetyHubTriggerSurvey(const base::Value::List& args);
  void HandleExtensionsSafetyHubExtensionKept(const base::Value::List& args);
  void HandleExtensionsSafetyHubExtensionRemoved(const base::Value::List& args);
  void HandleExtensionsSafetyHubNonTriggerExtensionRemoved(
      const base::Value::List& args);
  void HandleExtensionsSafetyHubRemoveAll(const base::Value::List& args);

  // content::WebContentsObserver overrides
  void PrimaryPageChanged(content::Page& page) override;

  // Requests the appropriate HaTS survey.
  void ExtensionsSafetyHubTriggerSurvey();

  SurveyStringData CreateSurveyStringsForNoInteraction();

  // Fill in needed extension metric data like the average age of the
  // extensions and the number of extensions installed.
  void InitExtensionStats();

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Requests a HaTS survey, and pass along needed string data.
  void RequestHatsSurvey(bool require_same_origin,
                         SurveyStringData string_data);

  // Functions for testing.
  void SetWebUI(content::WebUI* web_ui) { set_web_ui(web_ui); }
  void EnableNavigationForTest() { test_navigation_ = true; }

  // Metric variables to track user actions involving the extensions safety
  // check.
  int number_of_triggering_extensions_removed_ = 0;
  int number_of_nontriggering_extensions_removed_ = 0;
  int number_of_extensions_kept_ = 0;
  int number_installed_extensions_on_load_ = 0;
  std::string client_channel_;
  base::TimeDelta time_since_last_extension_install_;
  base::TimeDelta avg_extension_age_;
  base::Time time_extension_page_opened_;

  // Used to mimic a page navigation for testing.
  bool test_navigation_ = false;

  raw_ptr<Profile> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_HATS_HANDLER_H_
