// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_helpers.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/page_info/core/features.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "pdf/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/webui/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "content/public/common/content_features.h"
#endif  // BUILDFLAG(IS_ANDROID)

using content::WebContents;

namespace {

const char kTabContentsAttachedTabHelpersUserDataKey[] =
    "TabContentsAttachedTabHelpers";

}  // namespace

// static
// WARNING: Do not use this class for desktop chrome. Use TabFeatures instead.
// See
// https://chromium.googlesource.com/chromium/src/+/main/docs/chrome_browser_design_principles.md
void TabHelpers::AttachTabHelpers(WebContents* web_contents,
                                  bool enable_browser_autofill) {
  // If already adopted, nothing to be done.
  base::SupportsUserData::Data* adoption_tag =
      web_contents->GetUserData(&kTabContentsAttachedTabHelpersUserDataKey);
  if (adoption_tag) {
    return;
  }

  // Mark as adopted.
  web_contents->SetUserData(&kTabContentsAttachedTabHelpersUserDataKey,
                            std::make_unique<base::SupportsUserData::Data>());

  // Create all the tab helpers.

  // SessionTabHelper comes first because it sets up the tab ID, and other
  // helpers may rely on that.
  CreateSessionServiceTabHelper(web_contents);

  // ZoomController comes before common tab helpers since ChromeAutofillClient
  // may want to register as a ZoomObserver with it.
  zoom::ZoomController::CreateForWebContents(web_contents);

  // infobars::ContentInfoBarManager comes before common tab helpers since
  // ChromeSubresourceFilterClient has it as a dependency.
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // --- Section 1: Common tab helpers ---
  // AutofillClientProvider initializes ContentAutofillClient for web_contents,
  // which is gated by enable_browser_autofill.
  if (enable_browser_autofill) {
    autofill::AutofillClientProvider& autofill_client_provider =
        autofill::AutofillClientProviderFactory::GetForProfile(profile);
    autofill_client_provider.CreateClientForWebContents(web_contents);
  }

  // Password manager relies on ChromeAutofillClient initialized by browser
  // autofill, which is gated by enable_browser_autofill.
  if (enable_browser_autofill) {
    autofill::AutofillClientProvider& autofill_client_provider =
        autofill::AutofillClientProviderFactory::GetForProfile(profile);
    if (!autofill_client_provider.uses_platform_autofill()) {
      ChromePasswordManagerClient::CreateForWebContents(web_contents);
    }
  }

  // NO! Do not just add your tab helper here. This is a large alphabetized
  // block; please insert your tab helper above in alphabetical order.

  // --- Section 4: The warning ---

  // NONO    NO   NONONO   !
  // NO NO   NO  NO    NO  !
  // NO  NO  NO  NO    NO  !
  // NO   NO NO  NO    NO  !
  // NO    NONO   NONONO   !

  // Do NOT just drop your tab helpers here! There are three sections above (1.
  // All platforms, 2. Some platforms, 3. Behind BUILDFLAGs). Each is in rough
  // alphabetical order. PLEASE PLEASE PLEASE add your flag to the correct
  // section in the correct order.

  // This is common code for all of us. PLEASE DO YOUR PART to keep it tidy and
  // organized.
}
