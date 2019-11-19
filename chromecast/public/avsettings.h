// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_AVSETTINGS_H_
#define CHROMECAST_PUBLIC_AVSETTINGS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "chromecast_export.h"
#include "output_restrictions.h"
#include "task_runner.h"

namespace chromecast {

// Pure abstract interface to get and set media-related information. Each
// platform must provide its own implementation.
// All functions except constructor and destructor are called in one thread.
// All delegate functions can be called by platform implementation on any
// threads, for example, created by platform implementation internally.
class AvSettings {
 public:
  // Defines whether or not the cast receiver is the current active source of
  // the screen. If the device is connected to HDMI sinks, it may be unknown.
  // GENERATED_JAVA_ENUM_PACKAGE: com.google.android.apps.mediashell.avsettings
  enum ActiveState {
    UNKNOWN,
    STANDBY,   // Screen is off
    INACTIVE,  // Screen is on, but cast receiver is not active
    ACTIVE,    // Screen is on and cast receiver is active
  };

  // Audio codec supported by the device (or HDMI sink).
  // GENERATED_JAVA_ENUM_PACKAGE: com.google.android.apps.mediashell.avsettings
  enum AudioCodec {
    AC3 = 1 << 0,
    DTS = 1 << 1,
    DTS_HD = 1 << 2,
    EAC3 = 1 << 3,
    LPCM = 1 << 4,
    MPEG_H_AUDIO = 1 << 5,

    // All known audio codecs.
    ALL = AC3 | DTS | DTS_HD | EAC3 | LPCM | MPEG_H_AUDIO
  };

  // Defines the type of audio volume control of the device.
  // GENERATED_JAVA_ENUM_PACKAGE: com.google.android.apps.mediashell.avsettings
  enum AudioVolumeControlType {
    UNKNOWN_VOLUME,

    // MASTER_VOLUME: Devices of CEC audio controls is a master volume system,
    // i.e the system volume is changed, but not attenuated,
    // e.g. normal TVs, audio devices.
    MASTER_VOLUME,

    // ATTENUATION_VOLUME: Devices which do not do CEC audio controls,
    // e.g. Chromecast.
    ATTENUATION_VOLUME,

    // FIXED_VOLUME: Devices which have fixed volume, e.g. Nexus Player.
    FIXED_VOLUME,
  };

  enum class HdmiContentType {
    NO_DATA_TYPE,
    GAME_TYPE,
  };

  // Defines the status of platform wake-on-cast feature.
  enum WakeOnCastStatus {
    WAKE_ON_CAST_UNKNOWN,  // Should only been used very rarely when platform
                           // has error to get the status.
    WAKE_ON_CAST_NOT_SUPPORTED,  // Platform doesn't support wake-on-cast.
    WAKE_ON_CAST_DISABLED,
    WAKE_ON_CAST_ENABLED,
  };

  // GENERATED_JAVA_ENUM_PACKAGE: com.google.android.apps.mediashell.avsettings
  enum Event {
    // This event shall be fired whenever the active state is changed including
    // when the screen turned on, when the cast receiver (or the device where
    // cast receiver is running on) became the active input source, or after a
    // call to TurnActive() or TurnStandby().
    // WakeSystem() may change the active state depending on implementation.
    // On this event, GetActiveState() will be called on the thread where
    // Initialize() was called.
    ACTIVE_STATE_CHANGED = 0,

    // This event shall be fired whenever the audio codecs supported by the
    // device (or HDMI sinks connected to the device) are changed.
    // On this event, GetAudioCodecsSupported() and GetMaxAudioChannels() will
    // be called on the thread where Initialize() was called.
    AUDIO_CODECS_SUPPORTED_CHANGED = 2,

    // This event shall be fired whenever the screen information of the device
    // (or HDMI sinks connected to the device) are changed including screen
    // resolution, HDCP version and supported EOTFs.
    // On this event, GetScreenResolution(), GetHDCPVersion() and
    // GetSupportedEotfs(), GetScreenWidthMm(), GetScreenHeightMm() will be
    // called on the thread where Initialize() was called.
    SCREEN_INFO_CHANGED = 3,

    // This event should be fired whenever the active output restrictions on the
    // device outputs change. On this event, GetOutputRestrictions() will be
    // called on the thread where Initialize() was called.
    OUTPUT_RESTRICTIONS_CHANGED = 4,

    // This event shall be fired whenever the type of volume control provided
    // by the device is changed, for e.g., when the device is connected or
    // disconnected to HDMI sinks
    AUDIO_VOLUME_CONTROL_TYPE_CHANGED = 5,

    // This event shall be fired whenever wake-on-cast status is changed by
    // platform.
    WAKE_ON_CAST_CHANGED = 6,

    // This event shall be fired whenever the volume step interval provided
    // by the device is changed, for e.g. when connecting to an AVR setup
    // where step interval should be 1%.
    AUDIO_VOLUME_STEP_INTERVAL_CHANGED = 7,

    // This event shall be fired whenever the HDR output type changes.
    // On this event, GetHdrOutputType() will be called on the thread where
    // Initialize() was called.
    HDR_OUTPUT_TYPE_CHANGED = 8,

    // This event should be fired when the device is connected to HDMI sinks.
    HDMI_CONNECTED = 100,

    // This event should be fired when the device is disconnected to HDMI sinks.
    HDMI_DISCONNECTED = 101,

    // This event should be fired when an HDMI error occurs.
    HDMI_ERROR = 102,

    // This event should be fired when the display brightness is changed.
    DISPLAY_BRIGHTNESS_CHANGED = 200,
  };

  // Delegate to inform the caller events. As a subclass of TaskRunner,
  // AvSettings implementation can post tasks to the thread where Initialize()
  // was called.
  class Delegate : public TaskRunner {
   public:
    // This may be invoked to posts a task to the thread where Initialize() was
    // called.
    bool PostTask(Task* task, uint64_t delay_ms) override = 0;

    // This must be invoked to fire an event when one of the conditions
    // described above (Event) happens.
    virtual void OnMediaEvent(Event event) = 0;

    // This should be invoked when a key is pressed.
    // |key_code| is a CEC code defined in User Control Codes table of the CEC
    // specification (CEC Table 30 in the HDMI 1.4a specification).
    virtual void OnKeyPressed(int key_code) = 0;

    // This should be invoked when a key is released.
    virtual void OnKeyReleased(int key_code) = 0;

   protected:
    ~Delegate() override {}
  };

  virtual ~AvSettings() {}

  // Initializes avsettings and starts delivering events to |delegate|.
  // |delegate| must not be null.
  virtual void Initialize(Delegate* delegate) = 0;

  // Finalizes avsettings. It must assume |delegate| passed to Initialize() is
  // invalid after this call and stop delivering events.
  virtual void Finalize() = 0;

  // Returns current active state.
  virtual ActiveState GetActiveState() = 0;

  // Turns the screen on. Sets the active input to the cast receiver iff
  // switch_to_cast == true.
  // If successful, it must return true and fire ACTIVE_STATE_CHANGED.
  virtual bool TurnActive(bool switch_to_cast) = 0;

  // Turns the screen off (or stand-by). If the device is connecting to HDMI
  // sinks, broadcasts a CEC standby message on the HDMI control bus to put all
  // sink devices (TV, AVR) into a standby state.
  // If successful, it must return true and fire ACTIVE_STATE_CHANGED.
  virtual bool TurnStandby() = 0;

  // Requests the system where cast receiver is running on to be kept awake for
  // |time_ms|. If the system is already being kept awake, the period should be
  // extended from |time_ms| in the future.
  // It will be called when cast senders discover the cast receiver while the
  // system is in a stand-by mode (or a deeper sleeping/dormant mode depending
  // on the system). To respond to cast senders' requests, cast receiver needs
  // the system awake for given amount of time. The system should not turn
  // screen on.
  // Returns true if successful.
  virtual bool KeepSystemAwake(int time_ms) = 0;

  // Sets screen (backlight) brightness.
  // |brightness|: Range is 0.0 (off) to 1.0 (max).
  // |smooth|: If true, will gradually change brightness in a ramp. If true and
  // unsupported, returns false and does nothing. If false, sets brightness
  // immediately. If another ramp is already in progress, it is cancelled and a
  // new one is started from the current brightness of the display.
  // If the implementation rounds to discrete values, it should round up so that
  // non-0 |brightness| values don't turn off the display.
  // Returns false if set fails. Returns true otherwise.
  // Not all displays support this function.
  static CHROMECAST_EXPORT bool SetDisplayBrightness(float brightness,
                                                     bool smooth)
      __attribute__((weak));

  // Gets the current screen (backlight) brightness.
  // |brightness|: Range is 0.0 (off) to 1.0 (max).
  // Returns false and does not modify |brightness| if get fails.
  // Returns true and sets |brightness| to the current brightness otherwise.
  // Not all displays support this function.
  static CHROMECAST_EXPORT bool GetDisplayBrightness(float* brightness)
      __attribute__((weak));

  // Gets the nits output by the display at 100% brightness.
  // |nits|: The maximum brightness in nits.
  // Returns false and does not modify |nits| if get fails.
  // Returns true and sets |nits| on success.
  // Not all displays support this function.
  static CHROMECAST_EXPORT bool GetDisplayMaxBrightnessNits(float* nits)
      __attribute__((weak));

  // Set Hdmi content type. Return false if such operation fails. The operation
  // fails if unexpected errors occur, or if the desired |content_type| is not
  // supported by Hdmi sink, in which case implementation shall return false
  // without actually setting the content type.
  // This function should only be implemented on HDMI platforms.
  static CHROMECAST_EXPORT bool SetHdmiContentType(HdmiContentType content_type)
      __attribute__((weak));

  // Gets the HDMI latency in microseconds.
  // Returns valid values when HDMI is connected.
  // Returns 0 when HDMI is not connected or when the latency cannot be
  // measured.
  // This function should only be implemented on HDMI platforms.
  static CHROMECAST_EXPORT int GetHdmiLatencyUs() __attribute__((weak));

  // Returns the type of volume control, i.e. MASTER_VOLUME, FIXED_VOLUME or
  // ATTENUATION_VOLUME. For example, normal TVs, devices of CEC audio
  // controls, and audio devices are master volume systems. The counter
  // examples are Chromecast (which doesn't do CEC audio controls) and
  // Nexus Player which is fixed volume.
  virtual AudioVolumeControlType GetAudioVolumeControlType() = 0;

  // Retrieves the volume step interval in range [0.0, 1.0] that specifies how
  // much volume to change per step, e.g. 0.05 = 5%. Returns true if a valid
  // interval is specified by platform; returns false if interval should defer
  // to default values.
  //
  // Current default volume step intervals per control type are as follows:
  //  - MASTER_VOLUME: 0.05 (5%)
  //  - ATTENUATION_VOLUME: 0.02 (2%)
  //  - FIXED_VOLUME: 0.01 (1%)
  //  - UNKNOWN_VOLUME: 0.01 (1%)
  virtual bool GetAudioVolumeStepInterval(float* step_inteval) = 0;

  // Gets audio codecs supported by the device (or HDMI sinks).
  // The result is an integer of OR'ed AudioCodec values.
  virtual int GetAudioCodecsSupported() = 0;

  // Gets maximum number of channels for given audio codec, |codec|.
  virtual int GetMaxAudioChannels(AudioCodec codec) = 0;

  // Retrieves the resolution of screen of the device (or HDMI sinks).
  // Returns true if it gets resolution successfully.
  virtual bool GetScreenResolution(int* width, int* height) = 0;

  // Retrieves the refresh rate of screen of the device (or HDMI sinks) in
  // millihertz.
  // Returns true if it gets refresh rate successfully.
  // TODO(jiaqih): Update to virtual function in next API update.
  static CHROMECAST_EXPORT bool GetRefreshRateMillihertz(int* refresh_rate)
      __attribute__((weak));

  // Returns the current HDCP version multiplied by ten (so, for example, for
  // HDCP 2.2 the return value is 22). The return value should by 0 if HDCP is
  // not supported. Or TV_PLATFORM_NO_HDCP for platforms like CastTV that
  // support equivalent content protection without HDCP.
  enum { TV_PLATFORM_NO_HDCP = 99 };
  virtual int GetHDCPVersion() = 0;

  // Supported Electro-Optical Transfer Function (EOTF) reported by the device.
  // The values are according to Table 8 in CTA-861.3 (formerly CEA-861.3).
  // GENERATED_JAVA_ENUM_PACKAGE: com.google.android.apps.mediashell.avsettings
  enum Eotf {
    EOTF_SDR = 1 << 0,
    EOTF_HDR = 1 << 1,
    EOTF_SMPTE_ST_2084 = 1 << 2,
    EOTF_HLG = 1 << 3,
  };

  // Returns a set of flags, defined in the Eotf enum above, indicating support
  // of different EOTFs by the device or HDMI sink.
  virtual int GetSupportedEotfs() = 0;

  enum DolbyVisionCapFlags {
    DOLBY_SUPPORTED = 1 << 0,
    DOLBY_4K_P60_SUPPORTED = 1 << 1,
    DOLBY_422_12BIT_SUPPORTED = 1 << 2,
  };

  // Returns a set of flags, defined in the DolbyVisionCapFlags enum above,
  // indicating support for DolbyVision and various DV-related features.
  virtual int GetDolbyVisionFlags() = 0;

  // Returns physical screen size in millimeters.
  virtual int GetScreenWidthMm() = 0;
  virtual int GetScreenHeightMm() = 0;

  // If supported, retrieves the restrictions active on the device outputs (as
  // specified by the PlayReady CDM; see output_restrictions.h). If reporting
  // output restrictions is unsupported, should return false.
  virtual bool GetOutputRestrictions(
      OutputRestrictions* output_restrictions) = 0;

  // If supported, sets which output restrictions should be active on the device
  // (as specified by the PlayReady CDM; see output_restrictions.h). The device
  // should try to apply these restrictions and fire OUTPUT_RESTRICTIONS_CHANGED
  // if they result in a change of active restrictions.
  virtual void ApplyOutputRestrictions(
      const OutputRestrictions& restrictions) = 0;

  // Returns current Wake-On-Cast status from platform.
  virtual WakeOnCastStatus GetWakeOnCastStatus() = 0;

  // Enables/Disables Wake-On-Cast status.
  // Returns false if failed or not supported.
  virtual bool EnableWakeOnCast(bool enabled) = 0;

  // Supported HDR output modes.
  // GENERATED_JAVA_ENUM_PACKAGE: com.google.android.apps.mediashell.avsettings
  enum HdrOutputType {
    HDR_OUTPUT_SDR,  // not HDR
    HDR_OUTPUT_HDR,  // HDR with static metadata
    HDR_OUTPUT_DOLBYVISION  // DolbyVision output
  };

  // Gets the current HDR output type.
  virtual HdrOutputType GetHdrOutputType() = 0;

  // Sets the HDMI video mode according to the given parameters:
  // |allow_4k|: if false, the resolution set will not be a 4K resolution.
  // |optimize_for_fps|: *Attempts* to pick a refresh rate optimal for the
  // given content frame rate.  |optimize_for_fps| is expressed as framerate
  // * 100. I.e. 24hz -> 2400, 23.98hz -> 2398, etc.  Values <= 0 are ignored.
  // |output_type|: if set to HDR_OUTPUT_DOLBYVISION, the video mode set will
  // be a DV supported resolution. If set to HDR_OUTPUT_HDR, the video mode set
  // will be a 10-bit or greater video mode.
  //
  // Returns:
  // - true if HDMI video mode change is beginning.  Caller should wait for
  //   SCREEN_INFO_CHANGED event for mode change to complete.
  // - false if no HDMI video mode change has begun.  This could be because
  // HDMI is disconnected, or the current resolution is already good for the
  // given parameters, or no valid resolution with the given parameters is
  // found (ie. setting require_dolby_vision/require_hdr to true when the
  // sink doesn't support those features).
  //
  // Non-HDMI devices should return false.
  virtual bool SetHdmiVideoMode(bool allow_4k,
                                int optimize_for_fps,
                                HdrOutputType output_type) = 0;

  // Returns true if the HDMI sink supports the specified HDR output type in
  // the current HDMI mode.  Returns false otherwise.
  //
  // Non-HDMI devices should return false.
  virtual bool IsHdrOutputSupportedByCurrentHdmiVideoMode(
      HdrOutputType output_type) = 0;
};

// Entrypoint for overridable AvSettings shared library.
class CHROMECAST_EXPORT AvSettingsShlib {
 public:
  static AvSettings* Create(const std::vector<std::string>& argv);
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_AVSETTINGS_H_
