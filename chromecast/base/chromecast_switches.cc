// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/chromecast_switches.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace switches {

// Value indicating whether flag from command line switch is true.
const char kSwitchValueTrue[] = "true";

// Value indicating whether flag from command line switch is false.
const char kSwitchValueFalse[] = "false";

// Server url to upload crash data to.
// Default is "https://clients2.google.com/cr/report" for prod devices.
// Default is "https://clients2.google.com/cr/staging_report" for non prod.
const char kCrashServerUrl[] = "crash-server-url";

// Switch to enable daemon-mode in crash_uploader.
const char kCrashUploaderDaemon[] = "daemon";

// Switch to disable Crash reporting
const char kDisableCrashReporter[] = "disable-crash-reporter";

// Switch to disable Crashpad forwarding
const char kDisableCrashpadForwarding[] = "disable-crashpad-forwarding";

// Switch to dumpstate binary path.
const char kDumpstateBinPath[] = "dumpstate-path";

// Enable file accesses. It should not be enabled for most Cast devices.
const char kEnableLocalFileAccesses[] = "enable-local-file-accesses";

// Override the URL to which metrics logs are sent for debugging.
const char kOverrideMetricsUploadUrl[] = "override-metrics-upload-url";

// Only connect to WLAN interfaces.
const char kRequireWlan[] = "require-wlan";

// Pass the app id information to the renderer process, to be used for logging.
// last-launched-app should be the app that just launched and is spawning the
// renderer.
const char kLastLaunchedApp[] = "last-launched-app";
// previous-app should be the app that was running when last-launched-app
// started.
const char kPreviousApp[] = "previous-app";

// Flag indicating that a resource provider must be set up to provide cast
// receiver with resources. Apps cannot start until provided resources.
// This flag implies --alsa-check-close-timeout=0.
const char kAcceptResourceProvider[] = "accept-resource-provider";

// Name of the device the amp mixer should be opened on. If this flag is not
// specified it will default to the same device as kAlsaVolumeDeviceName.
const char kAlsaAmpDeviceName[] = "alsa-amp-device-name";

// Name of the simple mixer control element that the ALSA-based media library
// should use to toggle powersave mode on the system.
const char kAlsaAmpElementName[] = "alsa-amp-element-name";

// Time in ms to wait before closing the PCM handle when no more mixer inputs
// remain. Assumed to be 0 if --accept-resource-provider is present.
const char kAlsaCheckCloseTimeout[] = "alsa-check-close-timeout";

// Flag that enables resampling audio with sample rate below 32kHz up to 48kHz.
// Should be set to true for internal audio products.
const char kAlsaEnableUpsampling[] = "alsa-enable-upsampling";

// Optional flag to set a fixed sample rate for the alsa device.
// Deprecated: Use --audio-output-sample-rate instead.
const char kAlsaFixedOutputSampleRate[] = "alsa-fixed-output-sample-rate";

// Name of the device the mute mixer should be opened on. If this flag is not
// specified it will default to the same device as kAlsaVolumeDeviceName.
const char kAlsaMuteDeviceName[] = "alsa-mute-device-name";

// Name of the simple mixer control element that the ALSA-based media library
// should use to mute the system.
const char kAlsaMuteElementName[] = "alsa-mute-element-name";

// Minimum number of available frames for scheduling a transfer.
const char kAlsaOutputAvailMin[] = "alsa-output-avail-min";

// Size of the ALSA output buffer in frames. This directly sets the latency of
// the output device. Latency can be calculated by multiplying the sample rate
// by the output buffer size.
const char kAlsaOutputBufferSize[] = "alsa-output-buffer-size";

// Size of the ALSA output period in frames. The period of an ALSA output device
// determines how many frames elapse between hardware interrupts.
const char kAlsaOutputPeriodSize[] = "alsa-output-period-size";

// How many frames need to be in the output buffer before output starts.
const char kAlsaOutputStartThreshold[] = "alsa-output-start-threshold";

// Name of the device the volume control mixer should be opened on. Will use the
// same device as kAlsaOutputDevice and fall back to "default" if
// kAlsaOutputDevice is not supplied.
const char kAlsaVolumeDeviceName[] = "alsa-volume-device-name";

// Name of the simple mixer control element that the ALSA-based media library
// should use to control the volume.
const char kAlsaVolumeElementName[] = "alsa-volume-element-name";

// Number of audio output channels. This will be used to send audio buffer with
// specific number of channels to ALSA and generate loopback audio. Default
// value is 2.
const char kAudioOutputChannels[] = "audio-output-channels";

// Specify fixed sample rate for audio output stream. If this flag is not
// specified the StreamMixer will choose sample rate based on the sample rate of
// the media stream.
const char kAudioOutputSampleRate[] = "audio-output-sample-rate";

// Calibrated max output volume dBa for voice content at 1 meter, if known.
const char kMaxOutputVolumeDba1m[] = "max-output-volume-dba1m";

// Enable dynamically changing the channel count in the mixer depending on the
// input streams.
const char kMixerEnableDynamicChannelCount[] =
    "mixer-enable-dynamic-channel-count";

// Specify the start threshold frames for audio output when using our mixer.
// This is mostly used to override the default value to a larger value, for
// platforms that can't handle the default start threshold without running into
// audio underruns.
const char kMixerSourceAudioReadyThresholdMs[] =
    "mixer-source-audio-ready-threshold-ms";

// Specify the buffer size for audio output when using our mixer. This is mostly
// used to override the default value to a larger value, for platforms that
// can't handle an audio buffer so small without running into audio underruns.
const char kMixerSourceInputQueueMs[] = "mixer-source-input-queue-ms";

// Some platforms typically have very little 'free' memory, but plenty is
// available in buffers+cached.  For such platforms, configure this amount
// as the portion of buffers+cached memory that should be treated as
// unavailable.  If this switch is not used, a simple pressure heuristic based
// purely on free memory will be used.
const char kMemPressureSystemReservedKb[] = "mem-pressure-system-reserved-kb";

// Used to pass initial screen resolution to GPU process.  This allows us to set
// screen size correctly (so no need to resize when first window is created).
const char kCastInitialScreenWidth[] = "cast-initial-screen-width";
const char kCastInitialScreenHeight[] = "cast-initial-screen-height";
const char kGraphicsBufferCount[] = "graphics-buffer-count";

// Overrides the vsync interval used by the GPU process to refresh the display.
const char kVSyncInterval[] = "vsync-interval";

// When present, desktop cast_shell will create 1080p window (provided display
// resolution is high enough).  Otherwise, cast_shell defaults to 720p.
const char kDesktopWindow1080p[] = "desktop-window-1080p";

// When present overrides the screen resolution used by CanDisplayType API,
// instead of using the values obtained from avsettings.
const char kForceMediaResolutionHeight[] = "force-media-resolution-height";
const char kForceMediaResolutionWidth[] = "force-media-resolution-width";

// Enables input event handling by the window manager.
const char kEnableInput[] = "enable-input";

// Background color used when Chromium hasn't rendered anything yet.
const char kCastAppBackgroundColor[] = "cast-app-background-color";

// The number of pixels from the very left or right of the screen to consider as
// a valid origin for the left or right swipe gesture.  Overrides the default
// value in cast_system_gesture_handler.cc.
const char kSystemGestureStartWidth[] = "system-gesture-start-width";

// The number of pixels from the very top or bottom of the screen to consider as
// a valid origin for the top or bottom swipe gesture. Overrides the default
// value in cast_system_gesture_handler.cc.
const char kSystemGestureStartHeight[] = "system-gesture-start-height";

// The number of pixels up from the bottom of the screen to consider as a valid
// origin for a bottom swipe gesture. If set, overrides the value of both the
// above system-gesture-start-height flag and the default value in
// cast_system_gesture_handler.cc.
const char kBottomSystemGestureStartHeight[] = "bottom-gesture-start-height";

// The number of pixels from the start of a left swipe gesture to consider as a
// 'back' gesture.
const char kBackGestureHorizontalThreshold[] =
    "back-gesture-horizontal-threshold";

// Whether to enable detection and dispatch of a 'drag from the top' gesture.
const char kEnableTopDragGesture[] = "enable-top-drag-gesture";

// Whether in hospitality mode
const char kManagedMode[] = "managed-mode";

// Endpoint that the mixer service listens on. This is a path for a UNIX domain
// socket (default is /tmp/mixer-service).
const char kMixerServiceEndpoint[] = "mixer-service-endpoint";

// TCP port that the mixer service listens on on non-Linux platforms.
// (default 12854).
const char kMixerServicePort[] = "mixer-service-port";

extern const char kCastMemoryPressureCriticalFraction[] =
    "memory-pressure-critical-fraction";
extern const char kCastMemoryPressureModerateFraction[] =
    "memory-pressure-moderate-fraction";

// Rather than use the renderer hosted remotely in the media service, fall back
// to the default renderer within content_renderer. Does not change the behavior
// of the media service.
const char kDisableMojoRenderer[] = "disable-mojo-renderer";

// Forces the use of the mojo renderer. In other words, the renderer process
// will run a mojo renderer and CastRenderer will run in the browser process.
// This is necessary for devices that use CastRenderer.
//
// For this flag to have any effect, note that you must build the cast web
// runtime with the gn arg "enable_cast_renderer" set to true, and "renderer"
// must be included in the list "mojo_media_services".
//
// This flag has lower priority than "disable-mojo-renderer".
const char kForceMojoRenderer[] = "force-mojo-renderer";

// Per-product customization of force update UI remote url, also used in
// testing.
const char kForceUpdateRemoteUrl[] = "force-update-remote-url";

// System info file path. Default is an empty string, which
// means that dummy info will be used.
const char kSysInfoFilePath[] = "sys-info-file-path";

// Defer initialization of the base::FeatureList in an external service process,
// allowing the process to include its own non-default features.
const char kDeferFeatureList[] = "defer-feature-list";

// Rather than share a common pref config file with cast_service, use a
// dedicated browser pref config file. This must be set when `cast_browser` is
// running in a different process from `cast_service`.
const char kUseCastBrowserPrefConfig[] = "use-cast-browser-pref-config";

// Creates the service broker inside of this process. Only one process should
// host the service broker.
const char kInProcessBroker[] = "in-process-broker";

// Command-line arg to change the Unix domain socket path to connect to the
// Cast Mojo broker.
const char kCastMojoBrokerPath[] = "cast-mojo-broker-path";

}  // namespace switches

namespace chromecast {

bool GetSwitchValueBoolean(const std::string& switch_string,
                           const bool default_value) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_string)) {
    if (command_line->GetSwitchValueASCII(switch_string) !=
            switches::kSwitchValueTrue &&
        command_line->GetSwitchValueASCII(switch_string) !=
            switches::kSwitchValueFalse &&
        command_line->GetSwitchValueASCII(switch_string) != "") {
      LOG(WARNING) << "Invalid switch value " << switch_string << "="
                   << command_line->GetSwitchValueASCII(switch_string)
                   << "; assuming default value of " << default_value;
      return default_value;
    }
    return command_line->GetSwitchValueASCII(switch_string) !=
           switches::kSwitchValueFalse;
  }
  return default_value;
}

int GetSwitchValueInt(const std::string& switch_name, const int default_value) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_name)) {
    return default_value;
  }

  int arg_value;
  if (!base::StringToInt(command_line->GetSwitchValueASCII(switch_name),
                         &arg_value)) {
    LOG(DFATAL) << "--" << switch_name << " only accepts integers as arguments";
    return default_value;
  }
  return arg_value;
}

int GetSwitchValueNonNegativeInt(const std::string& switch_name,
                                 const int default_value) {
  DCHECK_GE(default_value, 0)
      << "--" << switch_name << " must have a non-negative default value";

  int value = GetSwitchValueInt(switch_name, default_value);
  if (value < 0) {
    LOG(DFATAL) << "--" << switch_name << " must have a non-negative value";
    return default_value;
  }
  return value;
}

double GetSwitchValueDouble(const std::string& switch_name,
                            const double default_value) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_name)) {
    return default_value;
  }

  double arg_value;
  if (!base::StringToDouble(command_line->GetSwitchValueASCII(switch_name),
                            &arg_value)) {
    LOG(DFATAL) << "--" << switch_name << " only accepts numbers as arguments";
    return default_value;
  }
  return arg_value;
}

uint32_t GetSwitchValueColor(const std::string& switch_name,
                             const uint32_t default_value) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_name)) {
    return default_value;
  }

  uint32_t arg_value = 0;
  if (!base::HexStringToUInt(
          command_line->GetSwitchValueASCII(switch_name).substr(1),
          &arg_value)) {
    LOG(ERROR) << "Invalid value for " << switch_name << " ("
               << command_line->GetSwitchValueASCII(switch_name)
               << "), using default.";
    return default_value;
  }
  return arg_value;
}

}  // namespace chromecast
