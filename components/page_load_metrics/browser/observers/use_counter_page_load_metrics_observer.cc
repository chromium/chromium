// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom.h"

using FeatureType = blink::mojom::UseCounterFeatureType;
using UkmFeatureList = UseCounterMetricsRecorder::UkmFeatureList;
using WebFeature = blink::mojom::WebFeature;
using WebDXFeature = blink::mojom::WebDXFeature;
using CSSSampleId = blink::mojom::CSSSampleId;
using PermissionsPolicyFeature = blink::mojom::PermissionsPolicyFeature;

namespace {

// It's always recommended to use the deprecation API in blink. If the feature
// was logged from the browser (or from both blink and the browser) where the
// deprecation API is not available, use this method for the console warning.
// Note that this doesn't generate the deprecation report.
void PossiblyWarnFeatureDeprecation(content::RenderFrameHost* rfh,
                                    WebFeature feature) {
  switch (feature) {
    case WebFeature::kDownloadInSandbox:
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "Download is disallowed. The frame initiating or instantiating the "
          "download is sandboxed, but the flag ‘allow-downloads’ is not set. "
          "See https://www.chromestatus.com/feature/5706745674465280 for more "
          "details.");
      return;
    case WebFeature::kDownloadInAdFrameWithoutUserGesture:
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "[Intervention] Download in ad frame without user activation is "
          "not allowed. See "
          "https://www.chromestatus.com/feature/6311883621531648 for more "
          "details.");
      return;

    default:
      return;
  }
}

template <size_t N>
bool TestAndSet(std::bitset<N>& bitset,
                blink::UseCounterFeature::EnumValue value) {
  bool has_record = bitset.test(value);
  bitset.set(value);
  return has_record;
}

}  // namespace

UseCounterMetricsRecorder::UseCounterMetricsRecorder(
    bool is_in_fenced_frames_page)
    : uma_features_(AtMostOnceEnumUmaDeferrer<blink::mojom::WebFeature>(
          "Blink.UseCounter.Features")),
      uma_main_frame_features_(
          AtMostOnceEnumUmaDeferrer<blink::mojom::WebFeature>(
              "Blink.UseCounter.MainFrame.Features")),
      uma_webdx_features_(AtMostOnceEnumUmaDeferrer<blink::mojom::WebDXFeature>(
          "Blink.UseCounter.WebDXFeatures")) {
  // Other instances are prepared only for non FencedFrames pages.
  if (is_in_fenced_frames_page) {
    return;
  }
  uma_css_properties_ =
      std::make_unique<AtMostOnceEnumUmaDeferrer<blink::mojom::CSSSampleId>>(
          "Blink.UseCounter.CSSProperties");
  uma_animated_css_properties_ =
      std::make_unique<AtMostOnceEnumUmaDeferrer<blink::mojom::CSSSampleId>>(
          "Blink.UseCounter.AnimatedCSSProperties");
  uma_permissions_policy_violation_enforce_ = std::make_unique<
      AtMostOnceEnumUmaDeferrer<blink::mojom::PermissionsPolicyFeature>>(
      "Blink.UseCounter.PermissionsPolicy.Violation.Enforce");
  uma_permissions_policy_allow2_ = std::make_unique<
      AtMostOnceEnumUmaDeferrer<blink::mojom::PermissionsPolicyFeature>>(
      "Blink.UseCounter.PermissionsPolicy.Allow2");
  uma_permissions_policy_header2_ = std::make_unique<
      AtMostOnceEnumUmaDeferrer<blink::mojom::PermissionsPolicyFeature>>(
      "Blink.UseCounter.PermissionsPolicy.Header2");
}

UseCounterMetricsRecorder::~UseCounterMetricsRecorder() = default;

void UseCounterMetricsRecorder::AssertNoMetricsRecordedOrDeferred() {
  // Verify that no feature usage is observed before commit
  DCHECK_EQ(uma_features_.recorded_or_deferred().count(), 0ul);
  DCHECK_EQ(uma_main_frame_features_.recorded_or_deferred().count(), 0ul);
  DCHECK_EQ(uma_webdx_features_.recorded_or_deferred().count(), 0ul);
  if (uma_css_properties_) {
    DCHECK_EQ(uma_css_properties_->recorded_or_deferred().count(), 0ul);
  }
  if (uma_animated_css_properties_) {
    DCHECK_EQ(uma_animated_css_properties_->recorded_or_deferred().count(),
              0ul);
  }
  if (uma_permissions_policy_violation_enforce_) {
    DCHECK_EQ(uma_permissions_policy_violation_enforce_->recorded_or_deferred()
                  .count(),
              0ul);
  }
  if (uma_permissions_policy_allow2_) {
    DCHECK_EQ(uma_permissions_policy_allow2_->recorded_or_deferred().count(),
              0ul);
  }
  if (uma_permissions_policy_header2_) {
    DCHECK_EQ(uma_permissions_policy_header2_->recorded_or_deferred().count(),
              0ul);
  }

  DCHECK_EQ(ukm_features_recorded_.count(), 0ul);
  DCHECK_EQ(webdev_metrics_ukm_features_recorded_.count(), 0ul);
}

void UseCounterMetricsRecorder::RecordUkmPageVisits(
    ukm::SourceId ukm_source_id) {
  auto web_feature_page_visit =
      static_cast<blink::UseCounterFeature::EnumValue>(WebFeature::kPageVisits);

  ukm::builders::Blink_UseCounter(ukm_source_id)
      .SetFeature(web_feature_page_visit)
      .SetIsMainFrameFeature(1)
      .Record(ukm::UkmRecorder::Get());
  ukm_features_recorded_.set(web_feature_page_visit);
}

void UseCounterMetricsRecorder::DisableDeferAndFlush() {
  uma_features_.DisableDeferAndFlush();
  uma_main_frame_features_.DisableDeferAndFlush();
  uma_webdx_features_.DisableDeferAndFlush();
  if (uma_css_properties_) {
    uma_css_properties_->DisableDeferAndFlush();
  }
  if (uma_animated_css_properties_) {
    uma_animated_css_properties_->DisableDeferAndFlush();
  }
  if (uma_permissions_policy_violation_enforce_) {
    uma_permissions_policy_violation_enforce_->DisableDeferAndFlush();
  }
  if (uma_permissions_policy_allow2_) {
    uma_permissions_policy_allow2_->DisableDeferAndFlush();
  }
  if (uma_permissions_policy_header2_) {
    uma_permissions_policy_header2_->DisableDeferAndFlush();
  }
}

void UseCounterMetricsRecorder::RecordOrDeferUseCounterFeature(
    content::RenderFrameHost* rfh,
    const blink::UseCounterFeature& feature) {
  switch (feature.type()) {
    case FeatureType::kWebFeature: {
      auto web_feature = static_cast<WebFeature>(feature.value());

      if (!uma_features_.IsRecordedOrDeferred(web_feature)) {
        PossiblyWarnFeatureDeprecation(rfh, web_feature);
        uma_features_.RecordOrDefer(web_feature);

        // For any WebFeature use counters that are mapped to a WebDXFeature,
        // record the WebDXFeature use counter as well.
        auto map = GetWebFeatureToWebDXFeatureMap();
        auto entry = map.find(web_feature);

        if (entry != map.end()) {
          uma_webdx_features_.RecordOrDefer(entry->second);
        }
      }
    } break;
    case FeatureType::kWebDXFeature:
      uma_webdx_features_.RecordOrDefer(
          static_cast<WebDXFeature>(feature.value()));
      break;
    // There are about 600 enums, so the memory required for a vector
    // histogram is about 600 * 8 bytes = 5KB 50% of the time there are about
    // 100 CSS properties recorded per page load. Storage in sparce
    // histogram entries are 48 bytes instead of 8 bytes so the memory
    // required for a sparse histogram is about 100 * 48 bytes = 5KB. On top
    // there will be std::map overhead and the acquire/release of a
    // base::Lock to protect the map during each update. Overall it is still
    // better to use a vector histogram here since it is faster to access
    // and merge and uses about same amount of memory.
    case FeatureType::kCssProperty:
      if (uma_css_properties_) {
        auto css_property = static_cast<CSSSampleId>(feature.value());

        if (!uma_css_properties_->IsRecordedOrDeferred(css_property)) {
          uma_css_properties_->RecordOrDefer(css_property);

          auto map = GetCSSProperties2WebDXFeatureMap();
          auto entry = map.find(css_property);

          if (entry != map.end() &&
              !uma_webdx_features_.IsRecordedOrDeferred(entry->second)) {
            uma_webdx_features_.RecordOrDefer(entry->second);
          }
        }
      }
      break;
    case FeatureType::kAnimatedCssProperty:
      if (uma_animated_css_properties_) {
        auto animated_css_property = static_cast<CSSSampleId>(feature.value());

        if (!uma_animated_css_properties_->IsRecordedOrDeferred(
                animated_css_property)) {
          uma_animated_css_properties_->RecordOrDefer(animated_css_property);

          auto map = GetAnimatedCSSProperties2WebDXFeatureMap();
          auto entry = map.find(animated_css_property);

          if (entry != map.end() &&
              !uma_webdx_features_.IsRecordedOrDeferred(entry->second)) {
            uma_webdx_features_.RecordOrDefer(entry->second);
          }
        }
      }
      break;
    case FeatureType::kPermissionsPolicyViolationEnforce:
      if (uma_permissions_policy_violation_enforce_) {
        uma_permissions_policy_violation_enforce_->RecordOrDefer(
            static_cast<PermissionsPolicyFeature>(feature.value()));
      }
      break;
    case FeatureType::kPermissionsPolicyHeader:
      if (uma_permissions_policy_header2_) {
        uma_permissions_policy_header2_->RecordOrDefer(
            static_cast<PermissionsPolicyFeature>(feature.value()));
      }
      break;
    case FeatureType::kPermissionsPolicyIframeAttribute:
      if (uma_permissions_policy_allow2_) {
        uma_permissions_policy_allow2_->RecordOrDefer(
            static_cast<PermissionsPolicyFeature>(feature.value()));
      }
      break;
  }
}

void UseCounterMetricsRecorder::RecordOrDeferMainFrameWebFeature(
    content::RenderFrameHost* rfh,
    blink::mojom::WebFeature web_feature) {
  // Don't check if the primary main frame of not, but just ignore sub-frame
  // cases as we record metrics also for non-primary main frame, e.g.
  // FencedFrames, if the instance is bound with the FencedFrames page.
  if (rfh->GetParent())
    return;

  uma_main_frame_features_.RecordOrDefer(web_feature);
}

void UseCounterMetricsRecorder::RecordWebFeatures(ukm::SourceId ukm_source_id) {
  for (WebFeature web_feature : GetAllowedUkmFeatures()) {
    auto feature_enum_value =
        static_cast<blink::UseCounterFeature::EnumValue>(web_feature);
    if (!uma_features_.IsRecordedOrDeferred(web_feature))
      continue;

    if (TestAndSet(ukm_features_recorded_, feature_enum_value))
      continue;

    ukm::builders::Blink_UseCounter(ukm_source_id)
        .SetFeature(feature_enum_value)
        .SetIsMainFrameFeature(
            uma_main_frame_features_.IsRecordedOrDeferred(web_feature))
        .Record(ukm::UkmRecorder::Get());
  }
  for (WebFeature web_feature : GetAllowedWebDevMetricsUkmFeatures()) {
    auto feature_enum_value =
        static_cast<blink::UseCounterFeature::EnumValue>(web_feature);
    if (!uma_features_.IsRecordedOrDeferred(web_feature))
      continue;

    if (TestAndSet(webdev_metrics_ukm_features_recorded_, feature_enum_value))
      continue;

    ukm::builders::Blink_DeveloperMetricsRare(ukm_source_id)
        .SetFeature(feature_enum_value)
        .SetIsMainFrameFeature(
            uma_main_frame_features_.IsRecordedOrDeferred(web_feature))
        .Record(ukm::UkmRecorder::Get());
  }
}

void UseCounterMetricsRecorder::RecordWebDXFeatures(
    ukm::SourceId ukm_source_id) {
  // Feed any used WebDXFeature counters to UKM. Due to our layering rules, we
  // can't easily use the WebDXFeature type in the UKM code, so pass our
  // WebDXFeatures as a set of int32_t's.
  std::set<int32_t> webdx_features;

  for (int32_t feature = 1;
       feature <= static_cast<int32_t>(WebDXFeature::kMaxValue); feature++) {
    if (uma_webdx_features_.IsRecordedOrDeferred(
            static_cast<WebDXFeature>(feature))) {
      webdx_features.insert(feature);
    }
  }

  ukm::UkmRecorder::Get()->RecordWebDXFeatures(
      ukm_source_id, webdx_features,
      static_cast<size_t>(WebDXFeature::kMaxValue));
}

const base::flat_map<blink::mojom::WebFeature, blink::mojom::WebDXFeature>&
UseCounterMetricsRecorder::GetWebFeatureToWebDXFeatureMap() {
  static const base::NoDestructor<
      const base::flat_map<WebFeature, WebDXFeature>>
      kMap{{
          {WebFeature::kViewTransition, WebDXFeature::kViewTransitions},
          {WebFeature::kValidPopoverAttribute, WebDXFeature::kPopover},
          {WebFeature::kCSSSubgridLayout, WebDXFeature::kSubgrid},
          {WebFeature::kCSSCascadeLayers, WebDXFeature::kCascadeLayers},
          // If the compression or decompression stream constructors were
          // invoked, WebDXFeature::count that as the CompressionStreams WebDX
          // feature being used.
          {WebFeature::kCompressionStreamConstructor,
           WebDXFeature::kCompressionStreams},
          {WebFeature::kDecompressionStreamConstructor,
           WebDXFeature::kCompressionStreams},
          {WebFeature::kAVIFImage, WebDXFeature::kAvif},
          {WebFeature::kBlockingAttributeRenderToken,
           WebDXFeature::kBlockingRender},
          {WebFeature::kV8BroadcastChannel_Constructor,
           WebDXFeature::kBroadcastChannel},
          {WebFeature::kCanvasRenderingContext2DContextLostEvent,
           WebDXFeature::kCanvasContextLost},
          {WebFeature::kCSSCascadeLayers, WebDXFeature::kCascadeLayers},
          {WebFeature::kPressureObserver_Constructor,
           WebDXFeature::kComputePressure},
          {WebFeature::kAdoptedStyleSheets,
           WebDXFeature::kConstructedStylesheets},
          {WebFeature::kCSSAtRuleContainer, WebDXFeature::kContainerQueries},
          {WebFeature::kCSSStyleContainerQuery,
           WebDXFeature::kContainerStyleQueries},
          {WebFeature::kCSSAtRuleCounterStyle, WebDXFeature::kCounterStyle},
          {WebFeature::kCreateCSSModuleScript, WebDXFeature::kCssModules},
          {WebFeature::kStreamingDeclarativeShadowDOM,
           WebDXFeature::kDeclarativeShadowDom},
          {WebFeature::kShowPickerSelect, WebDXFeature::kShowPickerSelect},
          {WebFeature::kHiddenUntilFoundAttribute,
           WebDXFeature::kHiddenUntilFoundAttribute},
          {WebFeature::kHTMLDetailsElementNameAttribute,
           WebDXFeature::kDetailsName},
          {WebFeature::kElementCheckVisibility,
           WebDXFeature::kElementCheckVisibility},
          {WebFeature::kHTMLUnsafeMethods, WebDXFeature::kParseHtmlUnsafe},
          {WebFeature::kHTMLSearchElement, WebDXFeature::kSearch},
          {WebFeature::kDialogElement, WebDXFeature::kDialog},
          {WebFeature::kV8DocumentPictureInPicture_RequestWindow_Method,
           WebDXFeature::kDocumentPictureInPicture},
          {WebFeature::kFlexGapSpecified, WebDXFeature::kFlexboxGap},
          {WebFeature::kCSSFlexibleBox, WebDXFeature::kFlexbox},
          {WebFeature::kCSSSelectorPseudoFocusVisible,
           WebDXFeature::kFocusVisible},
          {WebFeature::kCSSGridLayout, WebDXFeature::kGrid},
          {WebFeature::kCSSSelectorPseudoHas, WebDXFeature::kHas},
          {WebFeature::kIdleDetectionStart, WebDXFeature::kIdleDetection},
          {WebFeature::kImportMap, WebDXFeature::kImportMaps},
          {WebFeature::kIntersectionObserverV2,
           WebDXFeature::kIntersectionObserverV2},
          {WebFeature::kIntersectionObserver_Constructor,
           WebDXFeature::kIntersectionObserver},
          {WebFeature::kCSSSelectorPseudoIs, WebDXFeature::kIs},
          {WebFeature::kPrepareModuleScript, WebDXFeature::kJsModules},
          {WebFeature::kInstantiateModuleScript, WebDXFeature::kJsModules},
          {WebFeature::kV8MediaSession_Metadata_AttributeSetter,
           WebDXFeature::kMediaSession},
          {WebFeature::kOffscreenCanvas, WebDXFeature::kOffscreenCanvas},
          {WebFeature::kV8StorageManager_GetDirectory_Method,
           WebDXFeature::kOriginPrivateFileSystem},
          {WebFeature::kV8HTMLVideoElement_RequestPictureInPicture_Method,
           WebDXFeature::kPictureInPicture},
          {WebFeature::kElementRequestPointerLock, WebDXFeature::kPointerLock},
          {WebFeature::kCSSRelativeColor, WebDXFeature::kRelativeColor},
          {WebFeature::kCSSAtRuleScope, WebDXFeature::kScope},
          {WebFeature::kScrollend, WebDXFeature::kScrollend},
          {WebFeature::kTextFragmentAnchor,
           WebDXFeature::kScrollToTextFragment},
          {WebFeature::kV8HTMLInputElement_ShowPicker_Method,
           WebDXFeature::kShowPickerInput},
          {WebFeature::kHTMLSlotElement, WebDXFeature::kSlot},
          {WebFeature::kV8SpeechRecognition_Start_Method,
           WebDXFeature::kSpeechRecognition},
          {WebFeature::kV8SpeechSynthesis_Speak_Method,
           WebDXFeature::kSpeechSynthesis},
          {WebFeature::kStorageAccessAPI_HasStorageAccess_Method,
           WebDXFeature::kStorageAccess},
          {WebFeature::kStorageAccessAPI_requestStorageAccess_Method,
           WebDXFeature::kStorageAccess},
          {WebFeature::kStorageBucketsOpen, WebDXFeature::kStorageBuckets},
          {WebFeature::kCSSSelectorTargetText, WebDXFeature::kTargetText},
          {WebFeature::kHTMLTemplateElement, WebDXFeature::kTemplate},
          {WebFeature::kTextWrapBalance, WebDXFeature::kTextWrapBalance},
          {WebFeature::kTextWrapPretty, WebDXFeature::kTextWrapPretty},
          {WebFeature::kCSSSelectorUserValid, WebDXFeature::kUserPseudos},
          {WebFeature::kCSSSelectorUserInvalid, WebDXFeature::kUserPseudos},
          {WebFeature::kWebCodecs, WebDXFeature::kWebcodecs},
          {WebFeature::kHidDeviceOpen, WebDXFeature::kWebhid},
          {WebFeature::kV8LockManager_Request_Method, WebDXFeature::kWebLocks},
          {WebFeature::kWebPImage, WebDXFeature::kWebp},
          {WebFeature::kWebTransport, WebDXFeature::kWebtransport},
          {WebFeature::kUsbDeviceOpen, WebDXFeature::kWebusb},
          {WebFeature::kVTTCue, WebDXFeature::kWebvtt},
          {WebFeature::kCSSSelectorPseudoWhere, WebDXFeature::kWhere},
          {WebFeature::kDataListElement, WebDXFeature::kDatalist},
          {WebFeature::kCSSSelectorPseudoDir, WebDXFeature::kDirPseudo},
          {WebFeature::kHiddenUntilFoundAttribute,
           WebDXFeature::kHiddenUntilFound},
          {WebFeature::kAbortSignalAny, WebDXFeature::kAbortsignalAny},
          {WebFeature::kNavigationAPI, WebDXFeature::kNavigation},
          {WebFeature::kMathMLMathElement, WebDXFeature::kMathml},
          {WebFeature::kCanvasRenderingContext2DConicGradient,
           WebDXFeature::kCanvasCreateconicgradient},
          {WebFeature::kCanvasRenderingContext2DReset,
           WebDXFeature::kCanvasReset},
          {WebFeature::kCanvasRenderingContext2DRoundRect,
           WebDXFeature::kCanvasRoundrect},
          {WebFeature::kCSSColorMixFunction, WebDXFeature::kColorMix},
          {WebFeature::kImageSet, WebDXFeature::kImageSet},
          {WebFeature::kStructuredCloneMethod, WebDXFeature::kStructuredClone},
          {WebFeature::kSlotAssignNode, WebDXFeature::kSlotAssign},
          {WebFeature::kDeviceMotionSecureOrigin,
           WebDXFeature::kDeviceOrientationEvents},
          {WebFeature::kDeviceOrientationAbsoluteSecureOrigin,
           WebDXFeature::kDeviceOrientationEvents},
          {WebFeature::kDeviceOrientationSecureOrigin,
           WebDXFeature::kDeviceOrientationEvents},
          {WebFeature::kGamepadButtons, WebDXFeature::kDRAFT_Gamepad},
          {WebFeature::kWakeLockAcquireScreenLock,
           WebDXFeature::kScreenWakeLock},
          {WebFeature::kWakeLockAcquireSystemLock,
           WebDXFeature::kScreenWakeLock},
          {WebFeature::kWebBluetoothRemoteServerConnect,
           WebDXFeature::kWebBluetooth},
          {WebFeature::kWebNfcNdefReaderScan, WebDXFeature::kWebNfc},
          {WebFeature::kSerialPortOpen, WebDXFeature::kDRAFT_Serial},
          {WebFeature::kModuleDedicatedWorker, WebDXFeature::kJsModulesWorkers},
          {WebFeature::kModuleSharedWorker,
           WebDXFeature::kJsModulesSharedWorkers},
          {WebFeature::kCssDisplayPropertyMultipleValues,
           WebDXFeature::kTwoValueDisplay},
          {WebFeature::kTwoValuedOverflow, WebDXFeature::kOverflowShorthand},
          {WebFeature::kKeyboardApiGetLayoutMap, WebDXFeature::kKeyboardMap},
          {WebFeature::kSchedulerPostTask, WebDXFeature::kScheduler},
          {WebFeature::kSchedulerYield, WebDXFeature::kScheduler},
          {WebFeature::kTaskControllerConstructor, WebDXFeature::kScheduler},
          {WebFeature::kTaskControllerSetPriority, WebDXFeature::kScheduler},
          {WebFeature::kTaskSignalPriority, WebDXFeature::kScheduler},
          {WebFeature::kKeyboardApiLock, WebDXFeature::kKeyboardLock},
          {WebFeature::kAsyncClipboardAPIRead, WebDXFeature::kAsyncClipboard},
          {WebFeature::kAsyncClipboardAPIWrite, WebDXFeature::kAsyncClipboard},
          {WebFeature::kAsyncClipboardAPIReadText,
           WebDXFeature::kAsyncClipboard},
          {WebFeature::kAsyncClipboardAPIWriteText,
           WebDXFeature::kAsyncClipboard},
          {WebFeature::kHtmlClipboardApiUnsanitizedRead,
           WebDXFeature::kClipboardUnsanitizedFormats},
          {WebFeature::kV8AbortController_Constructor, WebDXFeature::kAborting},
          {WebFeature::kV8AbortSignal_Abort_Method, WebDXFeature::kAborting},
          {WebFeature::kAbortSignalTimeout, WebDXFeature::kAborting},
          {WebFeature::kEditContext, WebDXFeature::kEditContext},
          {WebFeature::kInertAttribute, WebDXFeature::kInert},
      }};

  return *kMap;
}

const base::flat_map<blink::mojom::CSSSampleId, blink::mojom::WebDXFeature>&
UseCounterMetricsRecorder::GetCSSProperties2WebDXFeatureMap() {
  static const base::NoDestructor<
      const base::flat_map<CSSSampleId, WebDXFeature>>
      kMap{{
          {CSSSampleId::kAccentColor, WebDXFeature::kAccentColor},
          {CSSSampleId::kAnchorName, WebDXFeature::kAnchorPositioning},
          {CSSSampleId::kAnimationComposition,
           WebDXFeature::kAnimationComposition},
          {CSSSampleId::kAppearance, WebDXFeature::kAppearance},
          {CSSSampleId::kAspectRatio, WebDXFeature::kAspectRatio},
          {CSSSampleId::kBackdropFilter, WebDXFeature::kBackdropFilter},
          {CSSSampleId::kBorderImage, WebDXFeature::kBorderImage},
          {CSSSampleId::kColorScheme, WebDXFeature::kColorScheme},
          {CSSSampleId::kContainIntrinsicSize,
           WebDXFeature::kContainIntrinsicSize},
          {CSSSampleId::kFieldSizing, WebDXFeature::kFieldSizing},
          {CSSSampleId::kFontOpticalSizing, WebDXFeature::kFontOpticalSizing},
          {CSSSampleId::kFontPalette, WebDXFeature::kFontPalette},
          {CSSSampleId::kFontSynthesisSmallCaps,
           WebDXFeature::kFontSynthesisSmallCaps},
          {CSSSampleId::kFontSynthesisStyle, WebDXFeature::kFontSynthesisStyle},
          {CSSSampleId::kFontSynthesisWeight,
           WebDXFeature::kFontSynthesisWeight},
          {CSSSampleId::kFontSynthesis, WebDXFeature::kFontSynthesis},
          {CSSSampleId::kFontVariantAlternates,
           WebDXFeature::kFontVariantAlternates},
          {CSSSampleId::kHyphens, WebDXFeature::kHyphens},
          {CSSSampleId::kScrollbarColor, WebDXFeature::kScrollbarColor},
          {CSSSampleId::kScrollbarGutter, WebDXFeature::kScrollbarGutter},
          {CSSSampleId::kScrollbarWidth, WebDXFeature::kScrollbarWidth},
          {CSSSampleId::kScrollSnapType, WebDXFeature::kScrollSnap},
          {CSSSampleId::kTextIndent, WebDXFeature::kTextIndent},
          {CSSSampleId::kTextSpacingTrim, WebDXFeature::kTextSpacingTrim},
          {CSSSampleId::kTransitionBehavior, WebDXFeature::kTransitionBehavior},
          {CSSSampleId::kTranslate, WebDXFeature::kIndividualTransforms},
          {CSSSampleId::kRotate, WebDXFeature::kIndividualTransforms},
          {CSSSampleId::kScale, WebDXFeature::kIndividualTransforms},
          {CSSSampleId::kWillChange, WebDXFeature::kWillChange},
          {CSSSampleId::kMaskImage, WebDXFeature::kMasks},
          {CSSSampleId::kMaskClip, WebDXFeature::kMasks},
          {CSSSampleId::kMaskSize, WebDXFeature::kMasks},
          {CSSSampleId::kMaskOrigin, WebDXFeature::kMasks},
          {CSSSampleId::kMaskRepeat, WebDXFeature::kMasks},
          {CSSSampleId::kMaskComposite, WebDXFeature::kMasks},
          {CSSSampleId::kMaskPosition, WebDXFeature::kMasks},
          {CSSSampleId::kMaskMode, WebDXFeature::kMasks},
          {CSSSampleId::kMask, WebDXFeature::kMasks},
          {CSSSampleId::kPaintOrder, WebDXFeature::kPaintOrder},
      }};

  return *kMap;
}

const base::flat_map<blink::mojom::CSSSampleId, blink::mojom::WebDXFeature>&
UseCounterMetricsRecorder::GetAnimatedCSSProperties2WebDXFeatureMap() {
  static const base::NoDestructor<
      const base::flat_map<CSSSampleId, WebDXFeature>>
      kMap{{
          // TODO(jstenback): This animated kFontPalette is being investigated.
          // Uncomment this once that's resolved, or replace this with something
          // else that matches the resolution of the investigation
          // {CSSSampleId::kFontPalette, WebDXFeature::kFontPaletteAnimation}
      }};

  return *kMap;
}

UseCounterPageLoadMetricsObserver::UseCounterPageLoadMetricsObserver() =
    default;

UseCounterPageLoadMetricsObserver::~UseCounterPageLoadMetricsObserver() =
    default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  recorder_ = std::make_unique<UseCounterMetricsRecorder>(
      /*is_in_fenced_frame_page=*/false);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Continue even if this instance is bound to a FencedFrames page. In such
  // cases, report only UKMs.
  recorder_ = std::make_unique<UseCounterMetricsRecorder>(
      /*is_in_fenced_frame_page=*/true);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Works as same as non prerendered case. UMAs/UKMs are not recorded for
  // cancelled prerendering.
  recorder_ = std::make_unique<UseCounterMetricsRecorder>(
      /*is_in_fenced_frame_page=*/false);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  recorder_->AssertNoMetricsRecordedOrDeferred();

  content::RenderFrameHost* rfh = navigation_handle->GetRenderFrameHost();

  // Each Page including FencedFrames Page will report with the SourceId that is
  // bound with the outermost main frame.
  if (!IsInPrerenderingBeforeActivation()) {
    recorder_->RecordUkmPageVisits(GetDelegate().GetPageUkmSourceId());
    recorder_->DisableDeferAndFlush();
  }

  recorder_->RecordOrDeferMainFrameWebFeature(rfh, WebFeature::kPageVisits);
  auto web_feature_page_visit =
      static_cast<blink::UseCounterFeature::EnumValue>(WebFeature::kPageVisits);
  recorder_->RecordOrDeferUseCounterFeature(
      rfh, {FeatureType::kWebFeature, web_feature_page_visit});

  auto css_total_pages_measured =
      static_cast<blink::UseCounterFeature::EnumValue>(
          CSSSampleId::kTotalPagesMeasured);
  recorder_->RecordOrDeferUseCounterFeature(
      rfh, {FeatureType::kCssProperty, css_total_pages_measured});
  recorder_->RecordOrDeferUseCounterFeature(
      rfh, {FeatureType::kAnimatedCssProperty, css_total_pages_measured});

  return CONTINUE_OBSERVING;
}

void UseCounterPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  auto begin = base::TimeTicks::Now();

  recorder_->RecordUkmPageVisits(GetDelegate().GetPageUkmSourceId());
  recorder_->DisableDeferAndFlush();

  auto end = base::TimeTicks::Now();
  base::TimeDelta elapsed = end - begin;
  // Records duration of DisableDeferAndFlush.
  base::UmaHistogramTimes(
      "PageLoad.Clients.UseCounter.Experimental."
      "MetricsReplayAtActivationDuration",
      elapsed);
}

void UseCounterPageLoadMetricsObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  for (const blink::UseCounterFeature& feature : features) {
    if (feature.type() == FeatureType::kWebFeature) {
      recorder_->RecordOrDeferMainFrameWebFeature(
          rfh, static_cast<WebFeature>(feature.value()));
    }
    recorder_->RecordOrDeferUseCounterFeature(rfh, feature);
  }
}

void UseCounterPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (IsInPrerenderingBeforeActivation())
    return;

  auto source_id = GetDelegate().GetPageUkmSourceId();
  recorder_->RecordWebDXFeatures(source_id);
  recorder_->RecordWebFeatures(source_id);
}

void UseCounterPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo&
        failed_provisional_load_info) {
  if (IsInPrerenderingBeforeActivation())
    return;

  auto source_id = GetDelegate().GetPageUkmSourceId();
  recorder_->RecordWebDXFeatures(source_id);
  recorder_->RecordWebFeatures(source_id);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (IsInPrerenderingBeforeActivation())
    return CONTINUE_OBSERVING;

  auto source_id = GetDelegate().GetPageUkmSourceId();
  recorder_->RecordWebDXFeatures(source_id);
  recorder_->RecordWebFeatures(source_id);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  if (mime_type == "image/svg+xml")
    return CONTINUE_OBSERVING;
  return PageLoadMetricsObserver::ShouldObserveMimeType(mime_type);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

bool UseCounterPageLoadMetricsObserver::IsInPrerenderingBeforeActivation()
    const {
  return (GetDelegate().GetPrerenderingState() ==
          page_load_metrics::PrerenderingState::kInPrerendering);
}
