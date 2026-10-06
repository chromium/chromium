// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_FACTORY_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_FACTORY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"
#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class WebContents;
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace payments::facilitated {

class FacilitatedPaymentsClient;

// Manages the lifetime of `ContentFacilitatedPaymentsDriver`. It is owned by
// `ContentFacilitatedPaymentsClient`. Creates one
// `ContentFacilitatedPaymentsDriver` for each outermost main `RenderFrameHost`.
class ContentFacilitatedPaymentsDriverFactory
    : public content::WebContentsObserver {
 public:
  ContentFacilitatedPaymentsDriverFactory(content::WebContents* web_contents,
                                          FacilitatedPaymentsClient* client);
  ContentFacilitatedPaymentsDriverFactory(
      const ContentFacilitatedPaymentsDriverFactory&) = delete;
  ContentFacilitatedPaymentsDriverFactory& operator=(
      const ContentFacilitatedPaymentsDriverFactory&) = delete;
  ~ContentFacilitatedPaymentsDriverFactory() override;

  // Gets or creates a dedicated `ContentFacilitatedPaymentsDriver` for the
  // `render_frame_host`. Drivers are only created for the outermost main frame.
  ContentFacilitatedPaymentsDriver& GetOrCreateForFrame(
      content::RenderFrameHost* render_frame_host);

  // Decides from the renderer's page `signals` whether to extract images from
  // `render_frame_host` for QR code decoding. Images are extracted only for the
  // active primary main frame, at most one evaluation at a time, and if there
  // is no QR code detected on the current page.
  virtual void OnHeuristicSignalsReported(
      content::RenderFrameHost* render_frame_host,
      const mojom::HeuristicSignals& signals);

  bool is_evaluating_qr_code_for_testing() const {
    return is_evaluating_qr_code_;
  }
  void set_has_detected_qr_code_for_testing(bool has_detected_qr_code) {
    has_detected_qr_code_ = has_detected_qr_code;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(
      ContentFacilitatedPaymentsDriverFactoryTest,
      IsEligibleForQrCodeDetection_FeatureDisabled_ReturnsFalse);
  FRIEND_TEST_ALL_PREFIXES(
      ContentFacilitatedPaymentsDriverFactoryTest,
      IsEligibleForQrCodeDetection_MerchantAllowlistedAndFeatureEnabled_ReturnsTrue);
  FRIEND_TEST_ALL_PREFIXES(
      ContentFacilitatedPaymentsDriverFactoryTest,
      IsEligibleForQrCodeDetection_MerchantNotAllowlisted_ReturnsFalse);
  FRIEND_TEST_ALL_PREFIXES(
      ContentFacilitatedPaymentsDriverFactoryTest,
      IsEligibleForQrCodeDetection_NoOptimizationGuideDecider_ReturnsFalse);
#if BUILDFLAG(IS_ANDROID)
  FRIEND_TEST_ALL_PREFIXES(
      ContentFacilitatedPaymentsDriverFactoryTest,
      OnTextCopiedToClipboard_ErrorDocument_DoesNotTriggerPixDetection_PixFlowExitedReasonLogged);
  FRIEND_TEST_ALL_PREFIXES(
      ContentFacilitatedPaymentsDriverFactoryTest,
      OnTextCopiedToClipboard_FrameNotActive_DoesNotTriggerPixDetection_PixFlowExitedReasonLogged);
  FRIEND_TEST_ALL_PREFIXES(
      ContentFacilitatedPaymentsDriverFactoryTest,
      OnTextCopiedToClipboard_PixCodeInIFrame_DoesNotTriggerPixDetection_PixFlowExitedReasonLogged);
  FRIEND_TEST_ALL_PREFIXES(
      ContentFacilitatedPaymentsDriverFactoryTest,
      OnTextCopiedToClipboard_PixCodeInIFrame_FlagEnabled_CorrectIframeUrlPassedToDriver);
  FRIEND_TEST_ALL_PREFIXES(
      ContentFacilitatedPaymentsDriverFactoryTest,
      OnTextCopiedToClipboard_PixCodeInIFrame_FlagEnabled_PixFlowExitedReasonNotLogged);
#endif  // BUILDFLAG(IS_ANDROID)
  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
#if BUILDFLAG(IS_ANDROID)
  // The Pix and payment link flows are only available on Android.
  void RenderFrameHostStateChanged(
      content::RenderFrameHost* render_frame_host,
      content::RenderFrameHost::LifecycleState old_state,
      content::RenderFrameHost::LifecycleState new_state) override;
  void OnTextCopiedToClipboard(content::RenderFrameHost* render_frame_host,
                               const std::u16string& copied_text) override;
#endif  // BUILDFLAG(IS_ANDROID)

  // Returns whether the `FacilitatedPaymentsAgent` should run QR code
  // heuristics on `url`. Detection requires both the feature flag and a
  // merchant on the Optimization Guide allowlist.
  bool IsEligibleForQrCodeDetection(const GURL& url) const;

  // Owns the drivers, one for each render frame host. Elements are erased in
  // RenderFrameDeleted(), or destroyed with `this` when the owning
  // `TabFeatures` is torn down before `WebContents`.
  base::flat_map<content::RenderFrameHost*,
                 std::unique_ptr<ContentFacilitatedPaymentsDriver>>
      driver_map_;

  // Per-page QR code state. Both are reset when the primary main frame commits
  // a new document.
  // True while images are being extracted and decoded. Prevents a page that
  // fires many layout or scroll events from starting overlapping extractions.
  bool is_evaluating_qr_code_ = false;
  // True once a QR code has been found. Stops detection for the rest of the
  // page.
  bool has_detected_qr_code_ = false;

  // Owner.
  const raw_ref<FacilitatedPaymentsClient> client_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_FACTORY_H_
