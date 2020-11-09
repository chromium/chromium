// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include "base/no_destructor.h"

// This file defines a list of UseCounter WebFeature measured in the
// UKM-based UseCounter. Features must all satisfy UKM privacy requirements
// (see go/ukm). In addition, features should only be added if it's shown
// (or highly likely be) rare, e.g. <1% of page views as measured by UMA.
//
// UKM-based UseCounter should be used to cover the case when UMA UseCounter
// data shows a behaviour that is rare but too common to blindly change.
// UKM-based UseCounter would allow use to find specific pages to reason about
// either a breaking change is acceptable or not.

using WebFeature = blink::mojom::WebFeature;

// UKM-based UseCounter features (WebFeature) should be defined in
// opt_in_features list.
const UseCounterPageLoadMetricsObserver::UkmFeatureList&
UseCounterPageLoadMetricsObserver::GetAllowedUkmFeatures() {
  static base::NoDestructor<UseCounterPageLoadMetricsObserver::UkmFeatureList>
      // We explicitly use an std::initializer_list below to work around GCC
      // bug 84849, which causes having a base::NoDestructor<T<U>> and passing
      // an initializer list of Us does not work.
      opt_in_features(std::initializer_list<WebFeature>({
          WebFeature::kNavigatorVibrate,
          WebFeature::kNavigatorVibrateSubFrame,
          WebFeature::kTouchEventPreventedNoTouchAction,
          WebFeature::kTouchEventPreventedForcedDocumentPassiveNoTouchAction,
          WebFeature::kApplicationCacheInstalledButNoManifest,
          WebFeature::kApplicationCacheManifestSelectInsecureOrigin,
          WebFeature::kApplicationCacheManifestSelectSecureOrigin,
          WebFeature::kMixedContentAudio,
          WebFeature::kMixedContentImage,
          WebFeature::kMixedContentVideo,
          WebFeature::kMixedContentPlugin,
          WebFeature::kOpenerNavigationWithoutGesture,
          WebFeature::kUsbRequestDevice,
          WebFeature::kXMLHttpRequestSynchronousInMainFrame,
          WebFeature::kXMLHttpRequestSynchronousInCrossOriginSubframe,
          WebFeature::kXMLHttpRequestSynchronousInSameOriginSubframe,
          WebFeature::kXMLHttpRequestSynchronousInWorker,
          WebFeature::kPaymentHandler,
          WebFeature::kPaymentRequestShowWithoutGesture,
          WebFeature::kHTMLImports,
          WebFeature::kHTMLImportsOnReverseOriginTrials,
          WebFeature::kElementCreateShadowRoot,
          WebFeature::kElementCreateShadowRootOnReverseOriginTrials,
          WebFeature::kDocumentRegisterElement,
          WebFeature::kDocumentRegisterElementOnReverseOriginTrials,
          WebFeature::kCredentialManagerCreatePublicKeyCredential,
          WebFeature::kCredentialManagerGetPublicKeyCredential,
          WebFeature::kCredentialManagerMakePublicKeyCredentialSuccess,
          WebFeature::kCredentialManagerGetPublicKeyCredentialSuccess,
          WebFeature::kU2FCryptotokenRegister,
          WebFeature::kU2FCryptotokenSign,
          // TODO(crbug.com/1129465): The below four use counters have high
          // usage, but are expected to be deprecated in several milestones.
          WebFeature::kElementAttachShadow,
          WebFeature::kElementAttachShadowOpen,
          WebFeature::kElementAttachShadowClosed,
          WebFeature::kCustomElementRegistryDefine,
          WebFeature::kTextToSpeech_Speak,
          WebFeature::kTextToSpeech_SpeakDisallowedByAutoplay,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetTop,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetLeft,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetRight,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetBottom,
          WebFeature::kMediaControlsDisplayCutoutGesture,
          WebFeature::kPolymerV1Detected,
          WebFeature::kPolymerV2Detected,
          WebFeature::kFullscreenSecureOrigin,
          WebFeature::kFullscreenInsecureOrigin,
          WebFeature::kPrefixedVideoEnterFullscreen,
          WebFeature::kPrefixedVideoExitFullscreen,
          WebFeature::kPrefixedVideoEnterFullScreen,
          WebFeature::kPrefixedVideoExitFullScreen,
          WebFeature::kDocumentLevelPassiveDefaultEventListenerPreventedWheel,
          WebFeature::kDocumentDomainBlockedCrossOriginAccess,
          WebFeature::kDocumentDomainEnabledCrossOriginAccess,
          WebFeature::kCursorImageGT32x32,
          WebFeature::kCursorImageLE32x32,
          WebFeature::kCursorImageGT64x64,
          WebFeature::kAdClick,
          WebFeature::kUpdateWithoutShippingOptionOnShippingAddressChange,
          WebFeature::kUpdateWithoutShippingOptionOnShippingOptionChange,
          WebFeature::kSignedExchangeInnerResponseInMainFrame,
          WebFeature::kSignedExchangeInnerResponseInSubFrame,
          WebFeature::kWebShareShare,
          WebFeature::kDownloadInAdFrameWithoutUserGesture,
          WebFeature::kOpenWebDatabase,
          WebFeature::kV8MediaCapabilities_DecodingInfo_Method,
          WebFeature::kOpenerNavigationDownloadCrossOrigin,
          WebFeature::kLinkRelPrerender,
          WebFeature::kAdClickNavigation,
          WebFeature::kV8HTMLVideoElement_RequestPictureInPicture_Method,
          WebFeature::kMediaCapabilitiesDecodingInfoWithKeySystemConfig,
          WebFeature::kTextFragmentAnchor,
          WebFeature::kTextFragmentAnchorMatchFound,
          WebFeature::kCookieInsecureAndSameSiteNone,
          WebFeature::kCookieStoreAPI,
          WebFeature::kDeviceOrientationSecureOrigin,
          WebFeature::kDeviceOrientationAbsoluteSecureOrigin,
          WebFeature::kDeviceMotionSecureOrigin,
          WebFeature::kRelativeOrientationSensorConstructor,
          WebFeature::kAbsoluteOrientationSensorConstructor,
          WebFeature::kLinearAccelerationSensorConstructor,
          WebFeature::kAccelerometerConstructor,
          WebFeature::kGyroscopeConstructor,
          WebFeature::kServiceWorkerInterceptedRequestFromOriginDirtyStyleSheet,
          WebFeature::kDownloadPrePolicyCheck,
          WebFeature::kDownloadPostPolicyCheck,
          WebFeature::kDownloadInAdFrame,
          WebFeature::kDownloadInSandbox,
          WebFeature::kDownloadWithoutUserGesture,
          WebFeature::kLazyLoadFrameLoadingAttributeLazy,
          WebFeature::kLazyLoadFrameLoadingAttributeEager,
          WebFeature::kLazyLoadImageLoadingAttributeLazy,
          WebFeature::kLazyLoadImageLoadingAttributeEager,
          WebFeature::kWebOTP,
          WebFeature::kBaseWithCrossOriginHref,
          WebFeature::kWakeLockAcquireScreenLock,
          WebFeature::kWakeLockAcquireSystemLock,
          WebFeature::kThirdPartyServiceWorker,
          WebFeature::kThirdPartySharedWorker,
          WebFeature::kThirdPartyBroadcastChannel,
          WebFeature::kHeavyAdIntervention,
          WebFeature::kGetGamepadsFromCrossOriginSubframe,
          WebFeature::kGetGamepadsFromInsecureContext,
          WebFeature::kGetGamepads,
          WebFeature::kMovedOrResizedPopup,
          WebFeature::kMovedOrResizedPopup2sAfterCreation,
          WebFeature::kDOMWindowOpenPositioningFeatures,
          WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton,
          WebFeature::kWebBluetoothRequestDevice,
          WebFeature::kWebBluetoothRequestScan,
          WebFeature::
              kV8VideoPlaybackQuality_CorruptedVideoFrames_AttributeGetter,
          WebFeature::kV8MediaSession_Metadata_AttributeSetter,
          WebFeature::kV8MediaSession_SetActionHandler_Method,
          WebFeature::kLargeStickyAd,
          WebFeature::
              kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive,
          WebFeature::kThirdPartyFileSystem,
          WebFeature::kThirdPartyIndexedDb,
          WebFeature::kThirdPartyCacheStorage,
          WebFeature::kOverlayPopup,
          WebFeature::kOverlayPopupAd,
          WebFeature::kTrustTokenXhr,
          WebFeature::kTrustTokenFetch,
          WebFeature::kTrustTokenIframe,
          WebFeature::kV8Document_HasTrustToken_Method,
          WebFeature::kV8HTMLVideoElement_RequestVideoFrameCallback_Method,
          WebFeature::kV8HTMLVideoElement_CancelVideoFrameCallback_Method,
          WebFeature::kSchemefulSameSiteContextDowngrade,
          WebFeature::kIdleDetectionStart,
          WebFeature::kPerformanceObserverEntryTypesAndBuffered,
          WebFeature::kStorageAccessAPI_HasStorageAccess_Method,
          WebFeature::kStorageAccessAPI_requestStorageAccess_Method,
          WebFeature::kThirdPartyCookieRead,
          WebFeature::kThirdPartyCookieWrite,
          WebFeature::kCrossSitePostMessage,
          WebFeature::kSchemelesslySameSitePostMessage,
          WebFeature::kSchemelesslySameSitePostMessageSecureToInsecure,
          WebFeature::kSchemelesslySameSitePostMessageInsecureToSecure,
          WebFeature::kElementAttachInternalsBeforeConstructor,
          WebFeature::kAddressSpaceLocalEmbeddedInPrivateSecureContext,
          WebFeature::kAddressSpaceLocalEmbeddedInPrivateNonSecureContext,
          WebFeature::kAddressSpaceLocalEmbeddedInPublicSecureContext,
          WebFeature::kAddressSpaceLocalEmbeddedInPublicNonSecureContext,
          WebFeature::kAddressSpaceLocalEmbeddedInUnknownSecureContext,
          WebFeature::kAddressSpaceLocalEmbeddedInUnknownNonSecureContext,
          WebFeature::kAddressSpacePrivateEmbeddedInPublicSecureContext,
          WebFeature::kAddressSpacePrivateEmbeddedInPublicNonSecureContext,
          WebFeature::kAddressSpacePrivateEmbeddedInUnknownSecureContext,
          WebFeature::kAddressSpacePrivateEmbeddedInUnknownNonSecureContext,
          WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation,
      }));
  return *opt_in_features;
}
