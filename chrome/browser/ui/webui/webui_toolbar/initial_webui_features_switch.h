// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_INITIAL_WEBUI_FEATURES_SWITCH_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_INITIAL_WEBUI_FEATURES_SWITCH_H_

namespace base {
class CommandLine;
}  // namespace base

namespace webui_toolbar {

// To update `kInitialWebUIEnableFeatures`, `kWebUIToolbarEnableFeatures`,
// and `kWebUIToolbarDisableFeatures`:
// 1. Edit chrome/browser/ui/webui/webui_toolbar/scripts/enabled.sh.
// 2. Run generate_chrome_flags.py in the same directory (which updates the
//    lists below, tools/perf/core/bot_platforms.py, and
//    tools/perf/core/shard_maps/*_map.json in-place).
// For `kWebUIToolbarEnableFeatures`, the script appends `WebUIToolbar` so
// that the chrome://flags entry turns on the entire WebUI toolbar.
// LINT.IfChange(WebUIToolbarEnableFeatures)
#define INITIAL_WEBUI_ENABLE_FEATURES                                         \
  "BypassOutdatedSurfaceActivation,"                                          \
  "DeferBackgroundTabLoadsWhileVisiblePageLoads,"                             \
  "DeferHistoryBackendInit,"                                                  \
  "DeferLayoutDuringBrowserStartup,"                                          \
  "DeferSessionStorageScavengingOnStartup,"                                   \
  "DeferSpellcheckInitialization,"                                            \
  "GpuPersistentCacheAsyncInitialize,"                                        \
  "GtkCacheNavButtonIcons,"                                                   \
  "GtkPrefetchNavButtonIcons,"                                                \
  "InitialWebUI,"                                                             \
  "InitialWebUIChromeOptimizations,"                                          \
  "InitialWebUISurfaceSync:deadline_in_frames/12000/"                         \
  "renderer_commit_delay_ms/100000,"                                          \
  "InitialWebUIWithoutExtensions,"                                            \
  "InitialWebUIWithoutSpellCheckForNtp,"                                      \
  "InitialWebUIWithoutTranslateForNtp,"                                       \
  "LazyKeyedServiceInstantiation,"                                            \
  "MacDeferInputContextUntilFirstPaint,"                                      \
  "MacPumpTasksBeforeFinishLaunching,"                                        \
  "OmniboxAimPopupDeferCreation,"                                             \
  "OmniboxWebUIPopupDeferCreation,"                                           \
  "OnlyUseWindowResizeHelperOnResize,"                                        \
  "OptimizeVariationsIpc,"                                                    \
  "PerDependencyDeadlines,"                                                   \
  "PrioritizeCompositorTaskRunnerOnStartup,"                                  \
  "PrioritizeResizeTaskRunnerOnStartup,"                                      \
  "PrioritizeWebUINavigationURLLoader,"                                       \
  "SendGPUChannelEarly,"                                                      \
  "SkipIPCChannelPausingForNonGuests,"                                        \
  "TopChromeWebUIPrerendering:TopChromeWebUIPrerenderingAtStartup/false/"     \
  "TopChromeWebUIPrerenderingUnbound/true,"                                   \
  "TopChromeWebUISharedRenderer,"                                             \
  "WebiumMetricsMapping:config/"                                              \
  "CjsKOUV2ZW50TGF0ZW5jeS5HZXN0dXJlU2Nyb2xsVXBkYXRlLlRvdWNoc2NyZWVuLlRvdGFs"  \
  "TGF0ZW5jeQooCiZQYWdlTG9hZC5JbnRlcmFjdGl2ZVRpbWluZy5JbnB1dERlbGF5Mwo4CjZH"  \
  "cmFwaGljcy5TbW9vdGhuZXNzLlBlcmNlbnREcm9wcGVkRnJhbWVzMy5BbGxTZXF1ZW5jZXM=," \
  "WebUIBackForwardButton,"                                                   \
  "WebUIBundledCodeCache,"                                                    \
  "WebUIBypassMojoConnections,"                                               \
  "WebUIHomeButton,"                                                          \
  "WebUIReloadButton:WebUIReloadButtonBypassLoaderThrottles/true/"            \
  "WebUIReloadButtonDeferBrowserViewShow/false/"                              \
  "WebUIReloadButtonKeepVisibleUntilPaint/true/"                              \
  "WebUIReloadButtonPrewarmWebUI/true/"                                       \
  "WebUIReloadButtonPrewarmWebUIPreNavigate/true/"                            \
  "WebUIReloadButtonProfilePrewarming/true/"                                  \
  "WebUIReloadButtonRestartUnresponsive/true/"                                \
  "WebUIReloadButtonRestartUnresponsiveRenderersTimeout/5s,"                  \
  "WebUISplitTabsButton,"                                                     \
  "WebUISyncDataPipeWrite,"                                                   \
  "WebUIToolbarFrameEvictionOptOut"
inline constexpr char kInitialWebUIEnableFeatures[] =
    INITIAL_WEBUI_ENABLE_FEATURES;
inline constexpr char kWebUIToolbarEnableFeatures[] =
    INITIAL_WEBUI_ENABLE_FEATURES ",WebUIToolbar";
#undef INITIAL_WEBUI_ENABLE_FEATURES
inline constexpr char kWebUIToolbarDisableFeatures[] =
    "BypassOutdatedSurfaceActivation,"
    "DeferBackgroundTabLoadsWhileVisiblePageLoads,"
    "DeferHistoryBackendInit,"
    "DeferLayoutDuringBrowserStartup,"
    "DeferSessionStorageScavengingOnStartup,"
    "DeferSpellcheckInitialization,"
    "GpuPersistentCacheAsyncInitialize,"
    "GtkCacheNavButtonIcons,"
    "GtkPrefetchNavButtonIcons,"
    "InitialWebUI,"
    "InitialWebUIChromeOptimizations,"
    "InitialWebUISurfaceSync,"
    "InitialWebUIWithoutExtensions,"
    "InitialWebUIWithoutSpellCheckForNtp,"
    "InitialWebUIWithoutTranslateForNtp,"
    "LazyKeyedServiceInstantiation,"
    "MacDeferInputContextUntilFirstPaint,"
    "MacPumpTasksBeforeFinishLaunching,"
    "OmniboxAimPopupDeferCreation,"
    "OmniboxWebUIPopupDeferCreation,"
    "OnlyUseWindowResizeHelperOnResize,"
    "OptimizeVariationsIpc,"
    "PerDependencyDeadlines,"
    "PrioritizeCompositorTaskRunnerOnStartup,"
    "PrioritizeResizeTaskRunnerOnStartup,"
    "PrioritizeWebUINavigationURLLoader,"
    "SendGPUChannelEarly,"
    "SkipIPCChannelPausingForNonGuests,"
    "TopChromeWebUIPrerendering,"
    "TopChromeWebUISharedRenderer,"
    "WebiumMetricsMapping,"
    "WebUIBackForwardButton,"
    "WebUIBundledCodeCache,"
    "WebUIBypassMojoConnections,"
    "WebUIHomeButton,"
    "WebUILocationBar,"
    "WebUIReloadButton,"
    "WebUISplitTabsButton,"
    "WebUISyncDataPipeWrite,"
    "WebUIToolbarFrameEvictionOptOut,"
    "WebUIToolbar";
// LINT.ThenChange(//chrome/browser/ui/webui/webui_toolbar/scripts/enabled.sh:WebUIToolbarEnableFeatures)

// Expands `--initial-webui-features=enabled|disabled` on `command_line` into
// `--enable-features` or `--disable-features`, preserving explicit caller
// feature overrides.
void ExpandInitialWebUIFeaturesSwitch(base::CommandLine& command_line);

}  // namespace webui_toolbar

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_INITIAL_WEBUI_FEATURES_SWITCH_H_
