// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_USER_DATA_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_USER_DATA_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/common/previews_state.h"

namespace previews {

// A representation of previews information related to a navigation.
// TODO(ryansturm): rename this to remove UserData.
class PreviewsUserData {
 public:
  explicit PreviewsUserData(uint64_t page_id);

  struct ServerLitePageInfo {
    std::unique_ptr<ServerLitePageInfo> Clone() {
      return std::make_unique<ServerLitePageInfo>(*this);
    }

    // The start time of the original navigation, that is, the one started by
    // the user.
    base::TimeTicks original_navigation_start = base::TimeTicks();

    // The page id used for this preview.
    uint64_t page_id = 0;

    // The DRP session key used for this preview.
    std::string drp_session_key = std::string();

    // The current state of the preview.
    ServerLitePageStatus status = ServerLitePageStatus::kUnknown;

    // The number of navigation restarts seen by this info instance.
    size_t restart_count = 0;
  };

  ~PreviewsUserData();

  PreviewsUserData(const PreviewsUserData& other);

  // A session unique ID related to this navigation.
  uint64_t page_id() const { return page_id_; }

  // The bool that is used in the coin flip holdback logic.
  bool CoinFlipForNavigation() const;

  // Sets the |reason| that the given |preview| was or was not shown in
  // |previews_eligibility_reasons_|.
  void SetEligibilityReasonForPreview(PreviewsType preview,
                                      PreviewsEligibilityReason reason);

  // Returns the reason that the given |preview| was or was not shown from
  // |previews_eligibility_reasons_|, if one exists.
  base::Optional<PreviewsEligibilityReason> EligibilityReasonForPreview(
      PreviewsType preview);

  // The effective connection type value for the navigation.
  net::EffectiveConnectionType navigation_ect() const {
    return navigation_ect_;
  }
  void set_navigation_ect(net::EffectiveConnectionType navigation_ect) {
    navigation_ect_ = navigation_ect;
  }

  // Whether the navigation was redirected from the original URL.
  bool is_redirect() const { return is_redirect_; }
  void set_is_redirect(bool is_redirect) { is_redirect_ = is_redirect; }

  // Returns the data savings inflation percent to use for this navigation
  // instead of the default if it is not 0.
  int data_savings_inflation_percent() const {
    return data_savings_inflation_percent_;
  }

  // Sets a data savings inflation percent value to use instead of the default
  // if there is a committed preview. Note that this is expected to be used for
  // specific preview types (such as NoScript) that don't have better data use
  // estimation information.
  void set_data_savings_inflation_percent(int inflation_percent) {
    data_savings_inflation_percent_ = inflation_percent;
  }

  // Whether a lite page preview was prevented from being shown due to the
  // blacklist.
  bool black_listed_for_lite_page() const {
    return black_listed_for_lite_page_;
  }
  void set_black_listed_for_lite_page(bool black_listed_for_lite_page) {
    black_listed_for_lite_page_ = black_listed_for_lite_page;
  }

  // Returns whether the Cache-Control:no-transform directive has been
  // detected for the request. Should not be called prior to receiving
  // a committed response.
  bool cache_control_no_transform_directive() const {
    return cache_control_no_transform_directive_;
  }
  // Sets that the page load received the Cache-Control:no-transform
  // directive. Expected to be set upon receiving a committed response.
  void set_cache_control_no_transform_directive() {
    cache_control_no_transform_directive_ = true;
  }

  // Whether there is a committed previews type.
  bool HasCommittedPreviewsType() const;

  // The committed previews type, if any. Otherwise, PreviewsType::NONE. This
  // should be used for metrics only since it does not respect the coin flip
  // holdback.
  previews::PreviewsType PreHoldbackCommittedPreviewsType() const;

  // The committed previews type, if any. Otherwise, PreviewsType::NONE. Returns
  // NONE if the coin flip holdback is kHoldback.
  previews::PreviewsType CommittedPreviewsType() const;

  // Sets the committed previews type. Should only be called once.
  void SetCommittedPreviewsType(previews::PreviewsType previews_type);
  // Sets the committed previews type for testing. Can be called multiple times.
  void SetCommittedPreviewsTypeForTesting(previews::PreviewsType previews_type);

  bool offline_preview_used() const { return offline_preview_used_; }
  // Whether an offline preview is being served.
  void set_offline_preview_used(bool offline_preview_used) {
    offline_preview_used_ = offline_preview_used;
  }

  // The PreviewsState that was allowed for the navigation. This should be used
  // for metrics only since it does not respect the coin flip holdback.
  content::PreviewsState PreHoldbackAllowedPreviewsState() const;

  // The PreviewsState that was allowed for the navigation. Returns PREVIEWS_OFF
  // if the coin flip holdback is kHoldback.
  content::PreviewsState AllowedPreviewsState() const;

  void set_allowed_previews_state(
      content::PreviewsState allowed_previews_state) {
    allowed_previews_state_without_holdback_ = allowed_previews_state;
  }

  // The PreviewsState that was committed for the navigation. This should be
  // used for metrics only since it does not respect the coin flip holdback.
  content::PreviewsState PreHoldbackCommittedPreviewsState() const;

  // The PreviewsState that was committed for the navigation. Returns
  // PREVIEWS_OFF if the coin flip holdback is kHoldback.
  content::PreviewsState CommittedPreviewsState() const;

  void set_committed_previews_state(
      content::PreviewsState committed_previews_state) {
    committed_previews_state_without_holdback_ = committed_previews_state;
  }

  // The result of a coin flip (if present) for this page load.
  CoinFlipHoldbackResult coin_flip_holdback_result() {
    return coin_flip_holdback_result_;
  }
  void set_coin_flip_holdback_result(
      CoinFlipHoldbackResult coin_flip_holdback_result) {
    coin_flip_holdback_result_ = coin_flip_holdback_result;
  }

  // Metadata for an attempted or committed Lite Page Redirect preview.
  ServerLitePageInfo* server_lite_page_info() {
    return server_lite_page_info_.get();
  }
  void set_server_lite_page_info(std::unique_ptr<ServerLitePageInfo> info) {
    server_lite_page_info_ = std::move(info);
  }

  // The serialized hints version for the hint that was used for the page load.
  base::Optional<std::string> serialized_hint_version_string() const {
    return serialized_hint_version_string_;
  }
  void set_serialized_hint_version_string(
      const std::string& serialized_hint_version_string) {
    serialized_hint_version_string_ = serialized_hint_version_string;
  }

 private:
  // A session unique ID related to this navigation.
  const uint64_t page_id_;

  // A random bool that is set once for a navigation and used in the coin flip
  // holdback logic.
  bool random_coin_flip_for_navigation_;

  // The effective connection type at the time of navigation. This is the value
  // to compare to the preview's triggering ect threshold.
  net::EffectiveConnectionType navigation_ect_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  // The navigation was redirected from the original URL.
  bool is_redirect_ = false;

  // A previews data savings inflation percent for the navigation if not 0.
  int data_savings_inflation_percent_ = 0;

  // Whether the origin provided a no-transform directive.
  bool cache_control_no_transform_directive_ = false;

  // Whether an offline preview is being served.
  bool offline_preview_used_ = false;

  // Whether a lite page preview was prevented from being shown due to the
  // blacklist.
  bool black_listed_for_lite_page_ = false;

  // The committed previews type, if any. Is not influenced by the coin flip
  // holdback.
  previews::PreviewsType committed_previews_type_without_holdback_ =
      PreviewsType::NONE;

  // The PreviewsState that was allowed for the navigation. Is not influenced by
  // the coin flip holdback.
  content::PreviewsState allowed_previews_state_without_holdback_ =
      content::PREVIEWS_UNSPECIFIED;

  // The PreviewsState that was committed for the navigation. Is not influenced
  // by the coin flip holdback.
  content::PreviewsState committed_previews_state_without_holdback_ =
      content::PREVIEWS_OFF;

  // The state of a random coin flip holdback, if any.
  CoinFlipHoldbackResult coin_flip_holdback_result_ =
      CoinFlipHoldbackResult::kNotSet;

  // Metadata for an attempted or committed Lite Page Redirect preview. See
  // struct comments for more detail.
  std::unique_ptr<ServerLitePageInfo> server_lite_page_info_;

  // A mapping from PreviewType to the last known reason why that preview type
  // was or was not triggered for this navigation. Used only for metrics.
  std::unordered_map<PreviewsType, PreviewsEligibilityReason>
      preview_eligibility_reasons_ = {};

  // The serialized hints version for the hint that was used for the page load.
  base::Optional<std::string> serialized_hint_version_string_ = base::nullopt;

  DISALLOW_ASSIGN(PreviewsUserData);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_USER_DATA_H_
