// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_INTERSTITIAL_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_INTERSTITIAL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "components/supervised_user/core/browser/supervised_user_error_page.h"
#include "url/gurl.h"

class PrefService;

namespace supervised_user {
class WebContentHandler;
class SupervisedUserService;

// This class is used by SupervisedUserNavigationObserver to handle requests
// from supervised user error page. The error page is shown when a page is
// blocked because it is on a denylist (in "allow everything" mode), not on any
// allowlist (in "allow only specified sites" mode), or doesn't pass safe
// search.
class SupervisedUserInterstitial {
 public:
  // The names of histograms emitted by this class.
  static constexpr char kInterstitialCommandHistogramName[] =
      "ManagedMode.BlockingInterstitialCommand";
  static constexpr char kInterstitialPermissionSourceHistogramName[] =
      "ManagedUsers.RequestPermissionSource";

  // For use in the kInterstitialCommandHistogramName histogram.
  //
  // The enum values should remain synchronized with the enum
  // ManagedModeBlockingCommand in tools/metrics/histograms/enums.xml.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Commands {
    // PREVIEW = 0,
    BACK = 1,
    // NTP = 2,
    REMOTE_ACCESS_REQUEST = 3,
    LOCAL_ACCESS_REQUEST = 4,
    HISTOGRAM_BOUNDING_VALUE = 5
  };

  // For use in the kInterstitialPermissionSourceHistogramName histogram.
  //
  // The enum values should remain synchronized with the
  // enum ManagedUserURLRequestPermissionSource in
  // tools/metrics/histograms/enums.xml.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RequestPermissionSource {
    MAIN_FRAME = 0,
    SUB_FRAME,
    HISTOGRAM_BOUNDING_VALUE
  };

  SupervisedUserInterstitial(const SupervisedUserInterstitial&) = delete;
  SupervisedUserInterstitial& operator=(const SupervisedUserInterstitial&) =
      delete;

  ~SupervisedUserInterstitial();

  static std::unique_ptr<SupervisedUserInterstitial> Create(
      std::unique_ptr<WebContentHandler> web_content_handler,
      SupervisedUserService& supervised_user_service,
      const GURL& url,
      const std::u16string& supervised_user_name,
      FilteringBehaviorReason reason);

  static std::string GetHTMLContents(
      SupervisedUserService* supervised_user_service,
      PrefService* pref_service,
      FilteringBehaviorReason reason,
      bool already_sent_request,
      bool is_main_frame,
      const std::string& application_locale);

  void GoBack();
  void RequestUrlAccessRemote(base::OnceCallback<void(bool)> callback);
  void RequestUrlAccessLocal(base::OnceCallback<void(bool)> callback);

  // Getter methods.
  const GURL& url() const { return url_; }
  WebContentHandler* web_content_handler() {
    return web_content_handler_.get();
  }
  FilteringBehaviorReason filtering_behavior_reason() const {
    return filtering_behavior_reason_;
  }

 private:
  SupervisedUserInterstitial(
      std::unique_ptr<WebContentHandler> web_content_handler,
      SupervisedUserService& supervised_user_service,
      const GURL& url,
      const std::u16string& supervised_user_name,
      FilteringBehaviorReason reason);

  void OutputRequestPermissionSourceMetric();

  const raw_ref<SupervisedUserService> supervised_user_service_;

  std::unique_ptr<WebContentHandler> web_content_handler_;

  // The last committed url for this frame.
  GURL url_;
  std::u16string supervised_user_name_;
  const FilteringBehaviorReason filtering_behavior_reason_;
  std::unique_ptr<UrlFormatter> url_formatter_;
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_INTERSTITIAL_H_
