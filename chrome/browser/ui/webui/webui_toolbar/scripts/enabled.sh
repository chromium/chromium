#! /bin/bash -e
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script outputs the enabled features and parameters needed to run chrome
# with WebUI top-chrome enabled. Use `run.sh`, not this directly.

. "$(dirname "$0")"/join.sh

# Please keep things in lexical order.

function InitialWebUIParams() {
  join / \
    high_stream_priority/true \
    without_spellcheck/true \
    without_translate/true \
    # empty line
}

function InitialWebUISurfaceSyncParams() {
  join / \
    deadline_in_frames/12000 \
    renderer_commit_delay_ms/100000 \
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
    WebUIReloadButtonDeferBrowserViewShow/false \
    WebUIReloadButtonKeepVisibleUntilPaint/true \
    WebUIReloadButtonPrewarmWebUI/true \
    WebUIReloadButtonPrewarmWebUIPreNavigate/true \
    # empty line
}

join , \
  BypassOutdatedSurfaceActivation \
  DeferHistoryBackendInit \
  DeferLayoutDuringBrowserStartup \
  DeferSessionStorageScavengingOnStartup \
  DeferSpellcheckInitialization \
  InitialWebUI:$(InitialWebUIParams) \
  InitialWebUISurfaceSync:$(InitialWebUISurfaceSyncParams) \
  InitialWebUIWithoutExtensions \
  LazyKeyedServiceInstantiation \
  PerDependencyDeadlines \
  PrioritizeResizeTaskRunnerOnStartup \
  SendGPUChannelEarly \
  SkipIPCChannelPausingForNonGuests \
  WebUIBackForwardButton \
  WebUIBundledCodeCache \
  WebUIBypassMojoConnections \
  WebUIHomeButton \
  WebUIReloadButton:$(WebUIReloadButtonParams) \
  WebUISplitTabsButton \
  WebUIToolbarFrameEvictionOptOut \
  WebiumMetricsMapping:$(WebiumMetricsMappingParams) \
  # empty line
