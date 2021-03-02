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
#include "components/previews/core/previews_block_list.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "third_party/blink/public/common/loader/previews_state.h"

namespace previews {

// A representation of previews information related to a navigation. We create
// a PreviewsUserData on every navigation, and when the navigation finishes, we
// make a copy of it and hold it in DocumentDataHolder so that the lifetime
// is tied to the document.
// For normal navigations, we will use the PreviewsUserData we made during the
// navigation.
// For navigations where we restore the page from the back-forward cache, we
// should instead used the cached PreviewsUserData that's associated with the
// document (the PreviewsUserData that's made during the original navigation
// the page, instead of the back navigation that triggers the restoration of
// the page so that the Previews UI will be consistent with the preserved
// page contents).
// TODO(ryansturm): rename this to remove UserData.
class PreviewsUserData {
 public:
  explicit PreviewsUserData(uint64_t page_id);

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
  // blocklist.
  bool block_listed_for_lite_page() const {
    return block_listed_for_lite_page_;
  }
  void set_block_listed_for_lite_page(bool block_listed_for_lite_page) {
    block_listed_for_lite_page_ = block_listed_for_lite_page;
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

  // The PreviewsState that was allowed for the navigation. This should be used
  // for metrics only since it does not respect the coin flip holdback.
  blink::PreviewsState PreHoldbackAllowedPreviewsState() const;

  // The PreviewsState that was allowed for the navigation. Returns PREVIEWS_OFF
  // if the coin flip holdback is kHoldback.
  blink::PreviewsState AllowedPreviewsState() const;

  void set_allowed_previews_state(blink::PreviewsState allowed_previews_state) {
    allowed_previews_state_without_holdback_ = allowed_previews_state;
  }

  // The PreviewsState that was committed for the navigation. This should be
  // used for metrics only since it does not respect the coin flip holdback.
  blink::PreviewsState PreHoldbackCommittedPreviewsState() const;

  // The PreviewsState that was committed for the navigation. Returns
  // PREVIEWS_OFF if the coin flip holdback is kHoldback.
  blink::PreviewsState CommittedPreviewsState() const;

  void set_committed_previews_state(
      blink::PreviewsState committed_previews_state) {
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

  void CopyData(const PreviewsUserData& other);

  class DocumentDataHolder
      : public content::RenderDocumentHostUserData<DocumentDataHolder> {
   public:
    ~DocumentDataHolder() override;
    PreviewsUserData* GetPreviewsUserData() {
      return previews_user_data_.get();
    }
    void SetPreviewsUserData(
        std::unique_ptr<PreviewsUserData> previews_user_data) {
      previews_user_data_ = std::move(previews_user_data);
    }

    using content::RenderDocumentHostUserData<
        DocumentDataHolder>::GetOrCreateForCurrentDocument;
    using content::RenderDocumentHostUserData<
        DocumentDataHolder>::GetForCurrentDocument;

   private:
    explicit DocumentDataHolder(content::RenderFrameHost* render_frame_host);
    friend class content::RenderDocumentHostUserData<DocumentDataHolder>;

    std::unique_ptr<PreviewsUserData> previews_user_data_;
    RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();
  };

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

  // Whether a lite page preview was prevented from being shown due to the
  // blocklist.
  bool block_listed_for_lite_page_ = false;

  // The committed previews type, if any. Is not influenced by the coin flip
  // holdback.
  previews::PreviewsType committed_previews_type_without_holdback_ =
      PreviewsType::NONE;

  // The PreviewsState that was allowed for the navigation. Is not influenced by
  // the coin flip holdback.
  blink::PreviewsState allowed_previews_state_without_holdback_ =
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED;

  // The PreviewsState that was committed for the navigation. Is not influenced
  // by the coin flip holdback.
  blink::PreviewsState committed_previews_state_without_holdback_ =
      blink::PreviewsTypes::PREVIEWS_OFF;

  // The state of a random coin flip holdback, if any.
  CoinFlipHoldbackResult coin_flip_holdback_result_ =
      CoinFlipHoldbackResult::kNotSet;

  // A mapping from PreviewType to the last known reason why that preview type
  // was or was not triggered for this navigation. Used only for metrics.
  std::unordered_map<PreviewsType, PreviewsEligibilityReason>
      preview_eligibility_reasons_ = {};

  DISALLOW_ASSIGN(PreviewsUserData);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_USER_DATA_H_
