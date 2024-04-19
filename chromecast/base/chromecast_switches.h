// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CHROMECAST_SWITCHES_H_
#define CHROMECAST_BASE_CHROMECAST_SWITCHES_H_

#include <cstdint>
#include <string>

#include "build/build_config.h"

namespace switches {

// Switch values
extern const char kSwitchValueTrue[];
extern const char kSwitchValueFalse[];

// Switches for crash_uploader
extern const char kCrashServerUrl[];
extern const char kCrashUploaderDaemon[];

// Disable Crash Reporting
extern const char kDisableCrashReporter[];

// Disable Crashpad forwarding
extern const char kDisableCrashpadForwarding[];

// Path to dumpstate binary to be used for crash reporting.
// This gives Chromium's crash reporter the flexibility to switch
// between multiple implementations of dumpstate tool.
extern const char kDumpstateBinPath[];

// Content-implementation switches
extern const char kEnableLocalFileAccesses[];

// Metrics switches
extern const char kOverrideMetricsUploadUrl[];

// Network switches
extern const char kRequireWlan[];

// Switches to communicate app state information
extern const char kLastLaunchedApp[];
extern const char kPreviousApp[];

// Cast Receiver switches
extern const char kAcceptResourceProvider[];

// ALSA-based CMA switches. (Only valid for audio products.)
// TODO(sergeyu): kAlsaEnableUpsampling and kAlsaCheckCloseTimeout are
// implemented in StreamMixer, which is not ALSA-specific - it's also used on
// Fuchsia. Rename these flags.
extern const char kAlsaAmpDeviceName[];
extern const char kAlsaAmpElementName[];
extern const char kAlsaCheckCloseTimeout[];
extern const char kAlsaEnableUpsampling[];
extern const char kAlsaFixedOutputSampleRate[];
extern const char kAlsaMuteDeviceName[];
extern const char kAlsaMuteElementName[];
extern const char kAlsaOutputAvailMin[];
extern const char kAlsaOutputBufferSize[];
extern const char kAlsaOutputPeriodSize[];
extern const char kAlsaOutputStartThreshold[];
extern const char kAlsaVolumeDeviceName[];
extern const char kAlsaVolumeElementName[];
extern const char kAudioOutputChannels[];
extern const char kAudioOutputSampleRate[];
extern const char kMaxOutputVolumeDba1m[];
extern const char kMixerEnableDynamicChannelCount[];
extern const char kMixerSourceAudioReadyThresholdMs[];
extern const char kMixerSourceInputQueueMs[];

// Memory pressure switches
extern const char kMemPressureSystemReservedKb[];

// GPU process switches
extern const char kCastInitialScreenWidth[];
extern const char kCastInitialScreenHeight[];
extern const char kGraphicsBufferCount[];
extern const char kVSyncInterval[];

// Graphics switches
extern const char kDesktopWindow1080p[];
extern const char kForceMediaResolutionHeight[];
extern const char kForceMediaResolutionWidth[];

// UI switches
extern const char kEnableInput[];
extern const char kSystemGestureStartWidth[];
extern const char kSystemGestureStartHeight[];
extern const char kBottomSystemGestureStartHeight[];
extern const char kBackGestureHorizontalThreshold[];
extern const char kEnableTopDragGesture[];
extern const char kManagedMode[];

// Background color used when Chromium hasn't rendered anything yet.
extern const char kCastAppBackgroundColor[];

extern const char kMixerServiceEndpoint[];
extern const char kMixerServicePort[];

extern const char kCastMemoryPressureCriticalFraction[];
extern const char kCastMemoryPressureModerateFraction[];

extern const char kDisableMojoRenderer[];
extern const char kForceMojoRenderer[];

// OTA update related switches.
extern const char kForceUpdateRemoteUrl[];

extern const char kSysInfoFilePath[];

// Switches for Cast browser decoupling.
extern const char kDeferFeatureList[];
extern const char kUseCastBrowserPrefConfig[];
extern const char kInProcessBroker[];

extern const char kCastMojoBrokerPath[];

}  // namespace switches

namespace chromecast {

// Gets boolean value from switch |switch_string|.
// --|switch_string| -> true
// --|switch_string|="true" -> true
// --|switch_string|="false" -> false
// no switch named |switch_string| -> |default_value|
bool GetSwitchValueBoolean(const std::string& switch_string,
                           const bool default_value);

// Gets an integer value from switch |switch_name|. If the switch is not present
// in the command line, or the value is not an integer, the |default_value| is
// returned.
int GetSwitchValueInt(const std::string& switch_name, const int default_value);

// Gets a non-negative integer value from switch |switch_name|. If the switch is
// not present in the command line, or the value is not a non-negative integer,
// the |default_value| is returned.
int GetSwitchValueNonNegativeInt(const std::string& switch_name,
                                 const int default_value);

// Gets a floating point value from switch |switch_name|. If the switch is not
// present in the command line, or the value is not a number, the
// |default_value| is returned.
double GetSwitchValueDouble(const std::string& switch_name,
                            const double default_value);

// Gets a color value from the format "#AARRGGBB" (hex).
uint32_t GetSwitchValueColor(const std::string& switch_name,
                             const uint32_t default_value);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_CHROMECAST_SWITCHES_H_
