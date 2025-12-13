// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_RENDERER_SECURITY_INTERSTITIAL_PAGE_CONTROLLER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_RENDERER_SECURITY_INTERSTITIAL_PAGE_CONTROLLER_H_

#include "components/security_interstitials/core/controller_client.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/public/wrappable_pointer_tags.h"
#include "gin/wrappable.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/cppgc/prefinalizer.h"

namespace content {
class RenderFrame;
}

namespace security_interstitials {

// This class makes various helper functions available to interstitials
// when committed interstitials are on. It is bound to the JavaScript
// window.certificateErrorPageController object.
class SecurityInterstitialPageController
    : public gin::Wrappable<SecurityInterstitialPageController>,
      public content::RenderFrameObserver {
  CPPGC_USING_PRE_FINALIZER(SecurityInterstitialPageController, Dispose);
 public:

  static constexpr gin::WrapperInfo kWrapperInfo = {
      {gin::kEmbedderNativeGin},
      gin::kSecurityInterstitialPageController};

  SecurityInterstitialPageController(
      const SecurityInterstitialPageController&) = delete;
  SecurityInterstitialPageController& operator=(
      const SecurityInterstitialPageController&) = delete;

  ~SecurityInterstitialPageController() override;

  // Creates an instance of SecurityInterstitialPageController which will invoke
  // SendCommand() in response to user actions taken on the interstitial page.
  static void Install(content::RenderFrame* render_frame);

 private:
  explicit SecurityInterstitialPageController(
      content::RenderFrame* render_frame);

  void Dispose();

  void DontProceed();
  void Proceed();
  void ShowMoreSection();
  void OpenHelpCenter();
  void OpenDiagnostic();
  void Reload();
  void OpenDateSettings();
  void OpenLogin();
  void DoReport();
  void DontReport();
  void OpenReportingPrivacy();
  void OpenWhitepaper();
  void ReportPhishingError();
  void OpenEnhancedProtectionSettings();
#if BUILDFLAG(IS_ANDROID)
  void OpenAdvancedProtectionSettings();
#endif  // BUILDFLAG(IS_ANDROID)
  void OpenHelpCenterInNewTab();
  void OpenDiagnosticInNewTab();
  void OpenReportingPrivacyInNewTab();
  void OpenWhitepaperInNewTab();
  void ReportPhishingErrorInNewTab();
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void ShowCertificateViewer();
#endif

  void SendCommand(security_interstitials::SecurityInterstitialCommand command);

  // gin::WrappableBase
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const gin::WrapperInfo* wrapper_info() const override;

  // RenderFrameObserver:
  void OnDestruct() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;

  // True if |this| forwards interstitial commands to the browser. This will be
  // set to false after any navigation.
  bool active_ = true;

  template <typename T>
  friend class cppgc::MakeGarbageCollectedTrait;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_RENDERER_SECURITY_INTERSTITIAL_PAGE_CONTROLLER_H_
