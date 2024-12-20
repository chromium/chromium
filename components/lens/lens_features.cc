// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace lens::features {

BASE_FEATURE(kLensStandalone,
             "LensStandalone",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensSearchOptimizations,
             "LensSearchOptimizations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLatencyLogging,
             "LensImageLatencyLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableRegionSearchKeyboardShortcut,
             "LensEnableRegionSearchKeyboardShortcut",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableImageTranslate,
             "LensEnableImageTranslate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableImageSearchSidePanelFor3PDse,
             "EnableImageSearchSidePanelFor3PDse",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensRegionSearchStaticPage,
             "LensRegionSearchStaticPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableContextMenuInLensSidePanel,
             "EnableContextMenuInLensSidePanel",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlay,
             "LensOverlay",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kLensOverlayTranslateButton,
             "LensOverlayTranslateButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayTranslateLanguages,
             "LensOverlayTranslateLanguages",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayImageContextMenuActions,
             "LensOverlayImageContextMenuActions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayContextualSearchbox,
             "LensOverlayContextualSearchbox",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayLatencyOptimizations,
             "LensOverlayLatencyOptimizations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayRoutingInfo,
             "LensOverlayRoutingInfo",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlaySurvey,
             "LensOverlaySurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kLensOverlayMinRamMb{&kLensOverlay, "min_ram_mb",
                                                   /*default=value=*/-1};
const base::FeatureParam<std::string> kActivityUrl{
    &kLensOverlay, "activity-url",
    "https://myactivity.google.com/myactivity?pli=1"};
const base::FeatureParam<std::string> kHelpCenterUrl{
    &kLensOverlay, "help-center-url",
    "https://support.google.com/chrome?p=search_from_page"};
const base::FeatureParam<std::string> kResultsSearchUrl{
    &kLensOverlay, "results-search-url", "https://www.google.com/search"};
const base::FeatureParam<int> kLensOverlayScreenshotRenderQuality{
    &kLensOverlay, "overlay-screenshot-render-quality", 90};
const base::FeatureParam<int> kLensOverlayImageCompressionQuality{
    &kLensOverlay, "image-compression-quality", 40};
const base::FeatureParam<bool> kLensOverlayUseTieredDownscaling{
    &kLensOverlay, "enable-tiered-downscaling", false};
const base::FeatureParam<bool> kLensOverlaySendLatencyGen204{
    &kLensOverlay, "enable-gen204-latency", true};
const base::FeatureParam<bool> kLensOverlaySendTaskCompletionGen204{
    &kLensOverlay, "enable-gen204-task-completion", true};
const base::FeatureParam<bool> kLensOverlaySendSemanticEventGen204{
    &kLensOverlay, "enable-gen204-semantic-event", true};
const base::FeatureParam<int> kLensOverlayImageMaxArea{
    &kLensOverlay, "image-dimensions-max-area", 1500000};
const base::FeatureParam<int> kLensOverlayImageMaxHeight{
    &kLensOverlay, "image-dimensions-max-height", 1600};
const base::FeatureParam<int> kLensOverlayImageMaxWidth{
    &kLensOverlay, "image-dimensions-max-width", 1600};
const base::FeatureParam<int> kLensOverlayImageMaxAreaTier1{
    &kLensOverlay, "image-dimensions-max-area-tier-1", 1000000};
const base::FeatureParam<int> kLensOverlayImageMaxHeightTier1{
    &kLensOverlay, "image-dimensions-max-height-tier-1", 1600};
const base::FeatureParam<int> kLensOverlayImageMaxWidthTier1{
    &kLensOverlay, "image-dimensions-max-width-tier-1", 1600};
const base::FeatureParam<int> kLensOverlayImageMaxAreaTier2{
    &kLensOverlay, "image-dimensions-max-area-tier-2", 2000000};
const base::FeatureParam<int> kLensOverlayImageMaxHeightTier2{
    &kLensOverlay, "image-dimensions-max-height-tier-2", 1890};
const base::FeatureParam<int> kLensOverlayImageMaxWidthTier2{
    &kLensOverlay, "image-dimensions-max-width-tier-2", 1890};
const base::FeatureParam<int> kLensOverlayImageMaxAreaTier3{
    &kLensOverlay, "image-dimensions-max-area-tier-3", 3000000};
const base::FeatureParam<int> kLensOverlayImageMaxHeightTier3{
    &kLensOverlay, "image-dimensions-max-height-tier-3", 2300};
const base::FeatureParam<int> kLensOverlayImageMaxWidthTier3{
    &kLensOverlay, "image-dimensions-max-width-tier-3", 2300};
const base::FeatureParam<int> kLensOverlayImageDownscaleUiScalingFactor{
    &kLensOverlay, "image-downscale-ui-scaling-factor", 2};
const base::FeatureParam<bool> kLensOverlayDebuggingMode{
    &kLensOverlay, "debugging-mode", false};
const base::FeatureParam<int> kLensOverlayVerticalTextMargin{
    &kLensOverlay, "text-vertical-margin", 12};
const base::FeatureParam<int> kLensOverlayHorizontalTextMargin{
    &kLensOverlay, "text-horizontal-margin", 4};
const base::FeatureParam<bool> kLensOverlaySearchBubble{&kLensOverlay,
                                                        "search-bubble", false};
const base::FeatureParam<bool> kLensOverlayEnableShimmer{
    &kLensOverlay, "enable-shimmer", true};
const base::FeatureParam<bool> kLensOverlayEnableShimmerSparkles{
    &kLensOverlay, "enable-shimmer-sparkles", true};
const base::FeatureParam<std::string> kResultsSearchLoadingUrl{
    &kLensOverlay, "results-search-loading-url",
    "https://www.gstatic.com/lens/chrome/"
    "lens_overlay_sidepanel_results_ghostloader_light-"
    "71af0ff0f00a1a03d3fe8abad71a2665.svg"};
const base::FeatureParam<std::string> kResultsSearchLoadingDarkModeUrl{
    &kLensOverlay, "results-search-loading-dark-mode-url",
    "https://www.gstatic.com/lens/chrome/"
    "lens_overlay_sidepanel_results_ghostloader_dark-"
    "b7b5c4f8c8891c881b7a20344f5298b0.svg"};
const base::FeatureParam<bool> kLensOverlayUseShimmerCanvas{
    &kLensOverlay, "use-shimmer-canvas", true};

const base::FeatureParam<bool> kLensOverlayGoogleDseRequired{
    &kLensOverlay, "google-dse-required", true};

const base::FeatureParam<bool> kUseLensOverlayForImageSearch{
    &kLensOverlay, "use-for-image-search", true};

const base::FeatureParam<bool> kUseLensOverlayForVideoFrameSearch{
    &kLensOverlay, "use-for-video-frame-search", true};

const base::FeatureParam<bool> kIsFindInPageEntryPointEnabled{
    &kLensOverlay, "find-in-page-entry-point", false};

const base::FeatureParam<bool> kIsOmniboxEntryPointEnabled{
    &kLensOverlay, "omnibox-entry-point", true};

constexpr base::FeatureParam<bool> kIsOmniboxEntrypointAlwaysVisible{
    &kLensOverlay, "omnibox-entry-point-always-visible", false};

const base::FeatureParam<bool> kUseBrowserDarkModeSettingForLensOverlay{
    &kLensOverlay, "use-browser-dark-mode-setting", true};

const base::FeatureParam<bool> kDynamicThemeForLensOverlay{
    &kLensOverlay, "use-dynamic-theme", true};

const base::FeatureParam<double> kDynamicThemeMinPopulationPct{
    &kLensOverlay, "use-dynamic-theme-min-population-pct", 0.002f};

const base::FeatureParam<double> kDynamicThemeMinChroma{
    &kLensOverlay, "use-dynamic-theme-min-chroma", 3.0f};

const base::FeatureParam<bool>
    kSendVisualSearchInteractionParamForLensTextQueries{
        &kLensOverlay, "send-vsint-for-text-selections", true};

constexpr base::FeatureParam<std::string> kLensOverlayEndpointUrl{
    &kLensOverlay, "endpoint-url",
    "https://lensfrontend-pa.googleapis.com/v1/crupload"};

constexpr base::FeatureParam<bool> kUseOauthForLensOverlayRequests{
    &kLensOverlay, "use-oauth-for-requests", true};

constexpr base::FeatureParam<int> kLensOverlayClusterInfoLifetimeSeconds{
    &kLensOverlay, "cluster-info-lifetime-seconds", 600};

constexpr base::FeatureParam<int> kLensOverlayTapRegionHeight{
    &kLensOverlay, "tap-region-height", 300};
constexpr base::FeatureParam<int> kLensOverlayTapRegionWidth{
    &kLensOverlay, "tap-region-width", 300};

constexpr base::FeatureParam<double>
    kLensOverlaySelectTextOverRegionTriggerThreshold{
        &kLensOverlay, "select-text-over-region-trigger-threshold", 0.1};

constexpr base::FeatureParam<int> kLensOverlaySignificantRegionMinArea{
    &kLensOverlay, "significant-regions-min-area", 500};

constexpr base::FeatureParam<int> kLensOverlayMaxSignificantRegions{
    &kLensOverlay, "max-significant-regions", 100};

constexpr base::FeatureParam<int> kLensOverlayLivePageBlurRadiusPixels{
    &kLensOverlay, "live-page-blur-radius-pixels", 200};

constexpr base::FeatureParam<bool> kLensOverlayUseCustomBlur{
    &kLensOverlay, "use-custom-blur", true};

constexpr base::FeatureParam<int> kLensOverlayCustomBlurBlurRadiusPixels{
    &kLensOverlay, "custom-blur-blur-radius-pixels", 60};

constexpr base::FeatureParam<double> kLensOverlayCustomBlurQuality{
    &kLensOverlay, "custom-blur-quality", 0.1};

constexpr base::FeatureParam<double> kLensOverlayCustomBlurRefreshRateHertz{
    &kLensOverlay, "custom-blur-refresh-rate-hertz", 30};

constexpr base::FeatureParam<double>
    kLensOverlayPostSelectionComparisonThreshold{
        &kLensOverlay, "post-selection-comparison-threshold", 0.005};

constexpr base::FeatureParam<int> kLensOverlayServerRequestTimeout{
    &kLensOverlay, "server-request-timeout", 10000};

constexpr base::FeatureParam<bool> kLensOverlayEnableErrorPage{
    &kLensOverlay, "enable-error-page-webui", true};

constexpr base::FeatureParam<std::string> kLensOverlayGscQueryParamValue{
    &kLensOverlay, "gsc-query-param-value", "2"};

const base::FeatureParam<bool> kLensOverlayEnableInFullscreen{
    &kLensOverlay, "enable-in-fullscreen", true};

constexpr base::FeatureParam<int> kLensOverlaySegmentationMaskCornerRadius{
    &kLensOverlay, "segmentation-mask-corner-radius", 12};

constexpr base::FeatureParam<int> kLensOverlayFindBarStringsVariant{
    &kLensOverlay, "find-bar-strings-variant", 0};

constexpr base::FeatureParam<bool>
    kLensOverlayImageContextMenuActionsEnableCopyAsImage{
        &kLensOverlayImageContextMenuActions, "enable-copy-as-image", false};

constexpr base::FeatureParam<bool>
    kLensOverlayImageContextMenuActionsEnableSaveAsImage{
        &kLensOverlayImageContextMenuActions, "enable-save-as-image", false};

constexpr base::FeatureParam<int>
    kLensOverlayImageContextMenuActionsTextReceivedTimeout{
        &kLensOverlayImageContextMenuActions, "text-received-timeout", 2000};

constexpr base::FeatureParam<bool> kEnableClusterInfoOptimization{
    &kLensOverlayLatencyOptimizations, "enable-cluster-info-optimization",
    true};

constexpr base::FeatureParam<bool> kEnableEarlyInteractionOptimization{
    &kLensOverlayLatencyOptimizations, "enable-early-interaction-optimization",
    true};

constexpr base::FeatureParam<bool> kEnableEarlyStartQueryFlowOptimization{
    &kLensOverlayLatencyOptimizations,
    "enable-early-start-query-flow-optimization", true};

constexpr base::FeatureParam<bool> kUsePdfsAsContext{
    &kLensOverlayContextualSearchbox, "use-pdfs-as-context", false};

constexpr base::FeatureParam<bool> kUseInnerTextAsContext{
    &kLensOverlayContextualSearchbox, "use-inner-text-as-context", false};

constexpr base::FeatureParam<bool> kUseInnerHtmlAsContext{
    &kLensOverlayContextualSearchbox, "use-inner-html-as-context", false};

constexpr base::FeatureParam<bool> kSendPageUrlForContextualization{
    &kLensOverlayContextualSearchbox, "send-page-url-for-contextualization",
    false};

constexpr base::FeatureParam<int> kLensOverlayPageContentRequestTimeoutMs{
    &kLensOverlayContextualSearchbox, "page-content-request-timeout-ms", 60000};

constexpr base::FeatureParam<bool>
    kUseVideoContextForTextOnlyLensOverlayRequests{
        &kLensOverlayContextualSearchbox,
        "use-video-context-for-text-only-requests", false};

constexpr base::FeatureParam<bool>
    kUseVideoContextForMultimodalLensOverlayRequests{
        &kLensOverlayContextualSearchbox,
        "use-video-context-for-multimodal-requests", false};

constexpr base::FeatureParam<std::string> kLensOverlayClusterInfoEndpointUrl{
    &kLensOverlayContextualSearchbox, "cluster-info-endpoint-url",
    "https://lensfrontend-pa.googleapis.com/v1/gsessionid"};

constexpr base::FeatureParam<bool>
    kLensOverlaySendLensInputsForContextualSuggest{
        &kLensOverlayContextualSearchbox,
        "send-lens-inputs-for-contextual-suggest", true};

constexpr base::FeatureParam<bool> kLensOverlaySendLensInputsForLensSuggest{
    &kLensOverlayContextualSearchbox, "send-lens-inputs-for-lens-suggest",
    false};

constexpr base::FeatureParam<bool>
    kShowContextualSearchboxGhostLoaderLoadingState{
        &kLensOverlayContextualSearchbox,
        "show-contextual-searchbox-ghost-loader-loading-state", true};

constexpr base::FeatureParam<bool> kShowContextualSearchboxSearchSuggest{
    &kLensOverlayContextualSearchbox,
    "show-contextual-searchbox-search-suggest", false};

constexpr base::FeatureParam<bool>
    kLensOverlaySendLensVisualInteractionDataForLensSuggest{
        &kLensOverlayContextualSearchbox,
        "send-lens-visual-interaction-data-for-lens-suggest", false};

constexpr base::FeatureParam<size_t> kLensOverlayFileUploadLimitBytes{
    &kLensOverlayContextualSearchbox, "file-upload-limit-bytes", 200000000};

constexpr base::FeatureParam<size_t> kLensOverlayPdfTextCharacterLimit{
    &kLensOverlayContextualSearchbox, "pdf-text-character-limit", 2500};

const base::FeatureParam<base::TimeDelta> kLensOverlaySurveyResultsTime{
    &kLensOverlaySurvey, "results-time", base::Seconds(1)};

constexpr base::FeatureParam<bool> kUsePdfVitParam{
    &kLensOverlayContextualSearchbox, "use-pdf-vit-param", false};

constexpr base::FeatureParam<bool> kUseWebpageVitParam{
    &kLensOverlayContextualSearchbox, "use-webpage-vit-param", false};

constexpr base::FeatureParam<bool> kUsePdfInteractionType{
    &kLensOverlayContextualSearchbox, "use-pdf-interaction-type", false};

constexpr base::FeatureParam<bool> kUseWebpageInteractionType{
    &kLensOverlayContextualSearchbox, "use-webpage-interaction-type", false};

constexpr base::FeatureParam<int> kScannedPdfCharacterPerPageHeuristic{
    &kLensOverlayContextualSearchbox, "characters-per-page-heuristic", 200};

constexpr base::FeatureParam<std::string> kTranslateEndpointUrl{
    &kLensOverlayTranslateLanguages, "translate-endpoint-url",
    "https://translate-pa.googleapis.com/v1/supportedLanguages"};
constexpr base::FeatureParam<std::string> kSupportedSourceTranslateLanguages{
    &kLensOverlayTranslateLanguages, "supported-source-languages",
    "aa,ab,ace,ach,af,ak,alz,am,ar,as,av,awa,ay,az,ba,ban,bbc,bci,be,bem,ber-"
    "Latn,bew,bg,bho,bik,bm,bn,bo,br,bs,bts,btx,bua,ca,ce,ceb,cgg,ch,chk,chm,"
    "ckb,cnh,co,crh,crs,cs,cv,cy,da,de,doi,dov,dyu,dz,ee,el,en,eo,es,et,eu,fa,"
    "fa-AF,ff,fi,fj,fo,fon,fr,fur,fy,ga,gaa,gd,gl,gn,gom,gu,gv,ha,haw,hi,hil,"
    "hmn,hr,hrx,ht,hu,hy,iba,id,ig,ilo,is,it,iw,ja,jam,jw,ka,kac,kek,kg,kha,kk,"
    "kl,km,kn,ko,kr,kri,ktu,ku,kv,ky,la,lb,lg,li,lij,lmo,ln,lo,lt,ltg,luo,lus,"
    "lv,mad,mai,mak,mam,mfe,mg,mh,mi,min,mk,ml,mn,mr,ms,ms-Arab,mt,mwr,my,ndc-"
    "ZW,ne,new,nhe,nl,no,nr,nso,nus,ny,oc,om,or,os,pa,pa-Arab,pag,pam,pap,pl,"
    "ps,pt,pt-PT,qu,rn,ro,rom,ru,rw,sa,sah,sat-Latn,scn,sd,se,sg,si,sk,sl,sm,"
    "sn,so,sq,sr,ss,st,su,sus,sv,sw,szl,ta,tcy,te,tet,tg,th,ti,tiv,tk,tl,tn,to,"
    "tpi,tr,trp,ts,tt,tum,ty,tyv,udm,ug,uk,ur,uz,ve,vec,vi,war,wo,xh,yi,yo,yua,"
    "yue,zap,zh-CN,zh-TW,zu"};
// To get the supported translate languages, we combine the source with these
// additional languages.
constexpr base::FeatureParam<std::string> kSupportedTargetTranslateLanguages{
    &kLensOverlayTranslateLanguages, "supported-target-languages",
    "bal,ber,bm-Nkoo,din,dv,mni-Mtei,shn"};
constexpr base::FeatureParam<base::TimeDelta> kSupportedLanguagesCacheTimeoutMs{
    &kLensOverlayTranslateLanguages, "supported-languages-cache-timeout-ms",
    base::Days(30)};
constexpr base::FeatureParam<int> kRecentLanguagesAmount{
    &kLensOverlayTranslateLanguages, "recent-languages-amount", 5};

constexpr base::FeatureParam<std::string> kHomepageURLForLens{
    &kLensStandalone, "lens-homepage-url", "https://lens.google.com/v3/"};

constexpr base::FeatureParam<bool> kEnableLensHtmlRedirectFix{
    &kLensStandalone, "lens-html-redirect-fix", false};

constexpr base::FeatureParam<bool>
    kDismissLoadingStateOnDocumentOnLoadCompletedInPrimaryMainFrame{
        &kLensStandalone,
        "dismiss-loading-state-on-document-on-load-completed-in-primary-main-"
        "frame",
        false};

constexpr base::FeatureParam<bool> kDismissLoadingStateOnDomContentLoaded{
    &kLensStandalone, "dismiss-loading-state-on-dom-content-loaded", false};

constexpr base::FeatureParam<bool> kDismissLoadingStateOnDidFinishNavigation{
    &kLensStandalone, "dismiss-loading-state-on-did-finish-navigation", false};

constexpr base::FeatureParam<bool>
    kDismissLoadingStateOnNavigationEntryCommitted{
        &kLensStandalone, "dismiss-loading-state-on-navigation-entry-committed",
        true};

constexpr base::FeatureParam<bool> kShouldIssuePreconnectForLens{
    &kLensStandalone, "lens-issue-preconnect", true};

constexpr base::FeatureParam<std::string> kPreconnectKeyForLens{
    &kLensStandalone, "lens-preconnect-key", "https://google.com"};

constexpr base::FeatureParam<bool> kShouldIssueProcessPrewarmingForLens{
    &kLensStandalone, "lens-issue-process-prewarming", true};

constexpr base::FeatureParam<bool> kDismissLoadingStateOnDidFinishLoad{
    &kLensStandalone, "dismiss-loading-state-on-did-finish-load", false};

constexpr base::FeatureParam<bool> kDismissLoadingStateOnPrimaryPageChanged{
    &kLensStandalone, "dismiss-loading-state-on-primary-page-changed", false};

const base::FeatureParam<bool> kEnableLensFullscreenSearch{
    &kLensSearchOptimizations, "enable-lens-fullscreen-search", false};

bool GetEnableLatencyLogging() {
  return base::FeatureList::IsEnabled(kEnableLatencyLogging) &&
         base::FeatureList::IsEnabled(kLensStandalone);
}

std::string GetHomepageURLForLens() {
  return kHomepageURLForLens.Get();
}

bool GetEnableLensHtmlRedirectFix() {
  return kEnableLensHtmlRedirectFix.Get();
}

bool GetDismissLoadingStateOnDocumentOnLoadCompletedInPrimaryMainFrame() {
  return kDismissLoadingStateOnDocumentOnLoadCompletedInPrimaryMainFrame.Get();
}

bool GetDismissLoadingStateOnDomContentLoaded() {
  return kDismissLoadingStateOnDomContentLoaded.Get();
}

bool GetDismissLoadingStateOnDidFinishNavigation() {
  return kDismissLoadingStateOnDidFinishNavigation.Get();
}

bool GetDismissLoadingStateOnNavigationEntryCommitted() {
  return kDismissLoadingStateOnNavigationEntryCommitted.Get();
}

bool GetDismissLoadingStateOnDidFinishLoad() {
  return kDismissLoadingStateOnDidFinishLoad.Get();
}

bool GetDismissLoadingStateOnPrimaryPageChanged() {
  return kDismissLoadingStateOnPrimaryPageChanged.Get();
}

bool GetEnableImageSearchUnifiedSidePanelFor3PDse() {
  return base::FeatureList::IsEnabled(kEnableImageSearchSidePanelFor3PDse);
}

bool IsLensFullscreenSearchEnabled() {
  return base::FeatureList::IsEnabled(kLensStandalone) &&
         base::FeatureList::IsEnabled(kLensSearchOptimizations) &&
         kEnableLensFullscreenSearch.Get();
}

bool IsLensSidePanelEnabled() {
  return base::FeatureList::IsEnabled(kLensStandalone);
}

bool IsLensRegionSearchStaticPageEnabled() {
  return base::FeatureList::IsEnabled(kLensRegionSearchStaticPage);
}

bool GetEnableContextMenuInLensSidePanel() {
  return base::FeatureList::IsEnabled(kEnableContextMenuInLensSidePanel);
}

bool GetShouldIssuePreconnectForLens() {
  return kShouldIssuePreconnectForLens.Get();
}

std::string GetPreconnectKeyForLens() {
  return kPreconnectKeyForLens.Get();
}

bool GetShouldIssueProcessPrewarmingForLens() {
  return kShouldIssueProcessPrewarmingForLens.Get();
}

bool IsLensOverlayEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlay);
}

std::string GetLensOverlayActivityURL() {
  return kActivityUrl.Get();
}

std::string GetLensOverlayHelpCenterURL() {
  return kHelpCenterUrl.Get();
}

int GetLensOverlayMinRamMb() {
  return kLensOverlayMinRamMb.Get();
}

std::string GetLensOverlayResultsSearchURL() {
  return kResultsSearchUrl.Get();
}

int GetLensOverlayImageCompressionQuality() {
  return kLensOverlayImageCompressionQuality.Get();
}

int GetLensOverlayScreenshotRenderQuality() {
  return kLensOverlayScreenshotRenderQuality.Get();
}

int GetLensOverlayImageMaxAreaTier1() {
  return kLensOverlayImageMaxAreaTier1.Get();
}

int GetLensOverlayImageMaxHeightTier1() {
  return kLensOverlayImageMaxHeightTier1.Get();
}

bool LensOverlayUseTieredDownscaling() {
  return kLensOverlayUseTieredDownscaling.Get();
}

bool GetLensOverlaySendLatencyGen204() {
  return kLensOverlaySendLatencyGen204.Get();
}

bool GetLensOverlaySendTaskCompletionGen204() {
  return kLensOverlaySendTaskCompletionGen204.Get();
}

bool GetLensOverlaySendSemanticEventGen204() {
  return kLensOverlaySendSemanticEventGen204.Get();
}

int GetLensOverlayImageMaxArea() {
  return kLensOverlayImageMaxArea.Get();
}

int GetLensOverlayImageMaxHeight() {
  return kLensOverlayImageMaxHeight.Get();
}

int GetLensOverlayImageMaxWidth() {
  return kLensOverlayImageMaxWidth.Get();
}

int GetLensOverlayImageMaxWidthTier1() {
  return kLensOverlayImageMaxWidthTier1.Get();
}

int GetLensOverlayImageMaxAreaTier2() {
  return kLensOverlayImageMaxAreaTier2.Get();
}

int GetLensOverlayImageMaxHeightTier2() {
  return kLensOverlayImageMaxHeightTier2.Get();
}

int GetLensOverlayImageMaxWidthTier2() {
  return kLensOverlayImageMaxWidthTier2.Get();
}

int GetLensOverlayImageMaxAreaTier3() {
  return kLensOverlayImageMaxAreaTier3.Get();
}

int GetLensOverlayImageMaxHeightTier3() {
  return kLensOverlayImageMaxHeightTier3.Get();
}

int GetLensOverlayImageMaxWidthTier3() {
  return kLensOverlayImageMaxWidthTier3.Get();
}

int GetLensOverlayImageDownscaleUiScalingFactorThreshold() {
  return kLensOverlayImageDownscaleUiScalingFactor.Get();
}

std::string GetLensOverlayEndpointURL() {
  return kLensOverlayEndpointUrl.Get();
}

bool IsLensOverlayDebuggingEnabled() {
  return kLensOverlayDebuggingMode.Get();
}

bool UseOauthForLensOverlayRequests() {
  return kUseOauthForLensOverlayRequests.Get();
}

int GetLensOverlayClusterInfoLifetimeSeconds() {
  return kLensOverlayClusterInfoLifetimeSeconds.Get();
}

bool UseVideoContextForTextOnlyLensOverlayRequests() {
  return kUseVideoContextForTextOnlyLensOverlayRequests.Get();
}

bool UseVideoContextForMultimodalLensOverlayRequests() {
  return kUseVideoContextForMultimodalLensOverlayRequests.Get();
}

std::string GetLensOverlayClusterInfoEndpointUrl() {
  return kLensOverlayClusterInfoEndpointUrl.Get();
}

bool GetLensOverlaySendLensInputsForContextualSuggest() {
  return kLensOverlaySendLensInputsForContextualSuggest.Get();
}

bool GetLensOverlaySendLensInputsForLensSuggest() {
  return kLensOverlaySendLensInputsForLensSuggest.Get();
}

bool GetLensOverlaySendLensVisualInteractionDataForLensSuggest() {
  return kLensOverlaySendLensVisualInteractionDataForLensSuggest.Get();
}

uint32_t GetLensOverlayFileUploadLimitBytes() {
  size_t limit = kLensOverlayFileUploadLimitBytes.Get();
  return base::IsValueInRangeForNumericType<uint32_t>(limit)
             ? static_cast<uint32_t>(limit)
             : 0;
}

uint32_t GetLensOverlayPdfSuggestCharacterTarget() {
  size_t limit = kLensOverlayPdfTextCharacterLimit.Get();
  return base::IsValueInRangeForNumericType<uint32_t>(limit)
             ? static_cast<uint32_t>(limit)
             : 0;
}

bool UsePdfVitParam() {
  return kUsePdfVitParam.Get();
}

bool UseWebpageVitParam() {
  return kUseWebpageVitParam.Get();
}

bool UsePdfInteractionType() {
  return kUsePdfInteractionType.Get();
}

bool UseWebpageInteractionType() {
  return kUseWebpageInteractionType.Get();
}

int GetScannedPdfCharacterPerPageHeuristic() {
  return kScannedPdfCharacterPerPageHeuristic.Get();
}

bool UsePdfsAsContext() {
  return kUsePdfsAsContext.Get();
}

bool UseInnerTextAsContext() {
  return kUseInnerTextAsContext.Get();
}

int GetLensOverlayPageContentRequestTimeoutMs() {
  return kLensOverlayPageContentRequestTimeoutMs.Get();
}

bool UseInnerHtmlAsContext() {
  return kUseInnerHtmlAsContext.Get();
}

bool SendPageUrlForContextualization() {
  return kSendPageUrlForContextualization.Get();
}

int GetLensOverlayVerticalTextMargin() {
  return kLensOverlayVerticalTextMargin.Get();
}

int GetLensOverlayHorizontalTextMargin() {
  return kLensOverlayHorizontalTextMargin.Get();
}

bool IsLensOverlaySearchBubbleEnabled() {
  return kLensOverlaySearchBubble.Get();
}

bool IsLensOverlayShimmerEnabled() {
  return kLensOverlayEnableShimmer.Get();
}

bool IsLensOverlayShimmerSparklesEnabled() {
  return kLensOverlayEnableShimmerSparkles.Get();
}

bool IsLensOverlayGoogleDseRequired() {
  return kLensOverlayGoogleDseRequired.Get();
}

std::string GetLensOverlayResultsSearchLoadingURL(bool dark_mode) {
  return dark_mode ? kResultsSearchLoadingDarkModeUrl.Get()
                   : kResultsSearchLoadingUrl.Get();
}

int GetLensOverlayTapRegionHeight() {
  return kLensOverlayTapRegionHeight.Get();
}

int GetLensOverlayTapRegionWidth() {
  return kLensOverlayTapRegionWidth.Get();
}

bool UseLensOverlayForImageSearch() {
  return kUseLensOverlayForImageSearch.Get();
}

bool UseLensOverlayForVideoFrameSearch() {
  return kUseLensOverlayForVideoFrameSearch.Get();
}

bool IsFindInPageEntryPointEnabled() {
  return kIsFindInPageEntryPointEnabled.Get();
}

bool IsOmniboxEntryPointEnabled() {
  return kIsOmniboxEntryPointEnabled.Get();
}

bool IsOmniboxEntrypointAlwaysVisible() {
  return kIsOmniboxEntrypointAlwaysVisible.Get();
}

bool UseBrowserDarkModeSettingForLensOverlay() {
  return kUseBrowserDarkModeSettingForLensOverlay.Get();
}

bool IsDynamicThemeDetectionEnabled() {
  return kDynamicThemeForLensOverlay.Get();
}

double DynamicThemeMinPopulationPct() {
  return kDynamicThemeMinPopulationPct.Get();
}

double DynamicThemeMinChroma() {
  return kDynamicThemeMinChroma.Get();
}

bool SendVisualSearchInteractionParamForLensTextQueries() {
  return kSendVisualSearchInteractionParamForLensTextQueries.Get();
}

double GetLensOverlaySelectTextOverRegionTriggerThreshold() {
  return kLensOverlaySelectTextOverRegionTriggerThreshold.Get();
}

bool GetLensOverlayUseShimmerCanvas() {
  return kLensOverlayUseShimmerCanvas.Get();
}

int GetLensOverlaySignificantRegionMinArea() {
  return kLensOverlaySignificantRegionMinArea.Get();
}

int GetLensOverlayMaxSignificantRegions() {
  return kLensOverlayMaxSignificantRegions.Get();
}

double GetLensOverlayPostSelectionComparisonThreshold() {
  return kLensOverlayPostSelectionComparisonThreshold.Get();
}

int GetLensOverlayLivePageBlurRadiusPixels() {
  return kLensOverlayLivePageBlurRadiusPixels.Get();
}

bool GetLensOverlayUseCustomBlur() {
  return kLensOverlayUseCustomBlur.Get();
}

int GetLensOverlayCustomBlurBlurRadiusPixels() {
  return kLensOverlayCustomBlurBlurRadiusPixels.Get();
}

double GetLensOverlayCustomBlurQuality() {
  return kLensOverlayCustomBlurQuality.Get();
}

double GetLensOverlayCustomBlurRefreshRateHertz() {
  return kLensOverlayCustomBlurRefreshRateHertz.Get();
}

int GetLensOverlayServerRequestTimeout() {
  return kLensOverlayServerRequestTimeout.Get();
}

bool GetLensOverlayEnableErrorPage() {
  return kLensOverlayEnableErrorPage.Get();
}

std::string GetLensOverlayGscQueryParamValue() {
  return kLensOverlayGscQueryParamValue.Get();
}

bool GetLensOverlayEnableInFullscreen() {
  return kLensOverlayEnableInFullscreen.Get();
}

int GetLensOverlaySegmentationMaskCornerRadius() {
  return kLensOverlaySegmentationMaskCornerRadius.Get();
}

int GetLensOverlayFindBarStringsVariant() {
  return kLensOverlayFindBarStringsVariant.Get();
}

bool IsLensOverlayTranslateButtonEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayTranslateButton);
}

bool IsLensOverlayCopyAsImageEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayImageContextMenuActions) &&
         kLensOverlayImageContextMenuActionsEnableCopyAsImage.Get();
}

bool IsLensOverlaySaveAsImageEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayImageContextMenuActions) &&
         kLensOverlayImageContextMenuActionsEnableSaveAsImage.Get();
}

int GetLensOverlayImageContextMenuActionsTextReceivedTimeout() {
  return kLensOverlayImageContextMenuActionsTextReceivedTimeout.Get();
}

bool IsLensOverlayContextualSearchboxEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayContextualSearchbox);
}

bool IsLensOverlayClusterInfoOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayLatencyOptimizations) &&
         kEnableClusterInfoOptimization.Get();
}

bool IsLensOverlayEarlyInteractionOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayLatencyOptimizations) &&
         kEnableEarlyInteractionOptimization.Get();
}

bool IsLensOverlayEarlyStartQueryFlowOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayLatencyOptimizations) &&
         kEnableEarlyStartQueryFlowOptimization.Get();
}

base::TimeDelta GetLensOverlaySurveyResultsTime() {
  return kLensOverlaySurveyResultsTime.Get();
}

bool IsLensOverlayTranslateLanguagesFetchEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayTranslateLanguages);
}

std::string GetLensOverlayTranslateEndpointURL() {
  return kTranslateEndpointUrl.Get();
}

bool ShowContextualSearchboxGhostLoaderLoadingState() {
  return kShowContextualSearchboxGhostLoaderLoadingState.Get();
}

std::string GetLensOverlayTranslateSourceLanguages() {
  return kSupportedSourceTranslateLanguages.Get();
}

std::string GetLensOverlayTranslateTargetLanguages() {
  return kSupportedTargetTranslateLanguages.Get();
}

base::TimeDelta GetLensOverlaySupportedLanguagesCacheTimeoutMs() {
  return kSupportedLanguagesCacheTimeoutMs.Get();
}

bool ShowContextualSearchboxSearchSuggest() {
  return kShowContextualSearchboxSearchSuggest.Get();
}

int GetLensOverlayTranslateRecentLanguagesAmount() {
  return kRecentLanguagesAmount.Get();
}

bool IsLensOverlayRoutingInfoEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayRoutingInfo);
}

}  // namespace lens::features
