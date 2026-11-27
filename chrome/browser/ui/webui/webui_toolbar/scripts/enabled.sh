#! /bin/bash -e
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script outputs the enabled features and parameters needed to run chrome
# with WebUI top-chrome enabled. Use `run.sh`, not this directly.

. "$(dirname "$0")"/join.sh

# Please keep things in lexical order.

function InitialWebUISurfaceSyncParams() {
  join / \
    deadline_in_frames/12000 \
    renderer_commit_delay_ms/100000 \
    # empty line
}

function TopChromeWebUIPrerenderingParams() {
  join / \
    TopChromeWebUIPrerenderingAtStartup/false \
    TopChromeWebUIPrerenderingUnbound/true \
    # empty line
}

function WebiumMetricsMappingParams() {
  join "" \
    "config/CjsKOUV2ZW50TGF0ZW5jeS5HZXN0dXJlU2Nyb2xsVXBkYXRlLlRvdWNoc2Ny" \
    "ZWVuLlRvdGFsTGF0ZW5jeQooCiZQYWdlTG9hZC5JbnRlcmFjdGl2ZVRpbWluZy5JbnB" \
    "1dERlbGF5Mwo4CjZHcmFwaGljcy5TbW9vdGhuZXNzLlBlcmNlbnREcm9wcGVkRnJhbW" \
    "VzMy5BbGxTZXF1ZW5jZXM="
}

function WebUIReloadButtonParams() {
  join / \
    WebUIReloadButtonBypassLoaderThrottles/true \
    WebUIReloadButtonDeferBrowserViewShow/false \
    WebUIReloadButtonKeepVisibleUntilPaint/true \
    WebUIReloadButtonPrewarmWebUI/true \
    WebUIReloadButtonPrewarmWebUIPreNavigate/true \
    WebUIReloadButtonProfilePrewarming/true \
    WebUIReloadButtonRestartUnresponsive/true \
    WebUIReloadButtonRestartUnresponsiveRenderersTimeout/5s \
    # empty line
}

# After editing this list, run `./generate_chrome_flags.py` (see `README.md`).
# LINT.IfChange(WebUIToolbarEnableFeatures)
join , \
  BypassOutdatedSurfaceActivation \
  DeferBackgroundTabLoadsWhileVisiblePageLoads \
  DeferHistoryBackendInit \
  DeferLayoutDuringBrowserStartup \
  DeferSessionStorageScavengingOnStartup \
  DeferSpellcheckInitialization \
  GpuPersistentCacheAsyncInitialize \
  GtkCacheNavButtonIcons \
  GtkPrefetchNavButtonIcons \
  InitialWebUI \
  InitialWebUIChromeOptimizations \
  InitialWebUISurfaceSync:$(InitialWebUISurfaceSyncParams) \
  InitialWebUIWithoutExtensions \
  InitialWebUIWithoutSpellCheckForNtp \
  InitialWebUIWithoutTranslateForNtp \
  LazyKeyedServiceInstantiation \
  MacDeferInputContextUntilFirstPaint \
  MacPumpTasksBeforeFinishLaunching \
  OmniboxAimPopupDeferCreation \
  OmniboxWebUIPopupDeferCreation \
  OnlyUseWindowResizeHelperOnResize \
  OptimizeVariationsIpc \
  PerDependencyDeadlines \
  PrioritizeCompositorTaskRunnerOnStartup \
  PrioritizeResizeTaskRunnerOnStartup \
  PrioritizeWebUINavigationURLLoader \
  SendGPUChannelEarly \
  SkipIPCChannelPausingForNonGuests \
  TopChromeWebUIPrerendering:$(TopChromeWebUIPrerenderingParams) \
  TopChromeWebUISharedRenderer \
  WebUIBackForwardButton \
  WebUIBundledCodeCache \
  WebUIBypassMojoConnections \
  WebUIHomeButton \
  WebUIReloadButton:$(WebUIReloadButtonParams) \
  WebUISplitTabsButton \
  WebUISyncDataPipeWrite \
  WebUIToolbarFrameEvictionOptOut \
  WebiumMetricsMapping:$(WebiumMetricsMappingParams) \
  # empty line
# LINT.ThenChange(//chrome/browser/ui/webui/webui_toolbar/initial_webui_features_switch.h:WebUIToolbarEnableFeatures)
