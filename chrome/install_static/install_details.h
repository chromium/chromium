// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALL_STATIC_INSTALL_DETAILS_H_
#define CHROME_INSTALL_STATIC_INSTALL_DETAILS_H_

#include <memory>
#include <string>

#include "base/win/windows_types.h"
#include "chrome/install_static/install_constants.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"

namespace install_static {

class PrimaryInstallDetails;
class ScopedInstallDetails;

// The origin of the active channel.
enum class ChannelOrigin {
  kInstallMode,           // The channel dictated by the install mode.
  kPolicy,                // The updater's "TargetChannel" policy.
};

// Details relating to how Chrome is installed. This class and
// PrimaryInstallDetails (below) are used in tandem so that one instance of the
// latter may be initialized early during process startup and then shared with
// other modules in the process. For example, chrome_elf creates the instance
// for a Chrome process and exports a GetInstallDetailsPayload function used by
// chrome.exe and chrome.dll to create their own module-specific instances
// referring to the same underlying payload. See install_modes.h for a gentle
// introduction to such terms as "brand" and "mode".
class InstallDetails {
 public:
  // A POD-struct containing the underlying data for an InstallDetails
  // instance. Multiple InstallDetails instances (e.g., one per module in a
  // process) share a single underlying Payload.
  struct Payload {
    // The size (in bytes) of this structure. This serves to verify that all
    // modules in a process have the same definition of the struct.
    size_t size;

    // The compile-time version of the product at the time that the process's
    // primary module was built. This is used to detect version skew across
    // modules in the process.
    const char* product_version;

    // The brand-specific install mode for this install; see kInstallModes.
    const InstallConstants* mode;

    // The friendly name of this Chrome's channel, or an empty string if the
    // brand does not integrate with Google Update.
    const wchar_t* channel;

    // The string length of |channel| (not including the string terminator).
    size_t channel_length;

    // The origin of the |channel| value. Install modes that use the FLOATING
    // channel strategy may determine the channel by an administrative policy
    // override (kPolicy). For all other install modes, the channel is dictated
    // by the mode itself (kInstallMode).
    ChannelOrigin channel_origin;

    // The value that was used to select |channel| if |channel_origin| is
    // kPolicy. This is the value provided to the installer via the --channel=
    // command line switch or the value provided to the browser via the
    // "channel" value in its Clients key.
    const wchar_t* channel_override;

    // The "ap" (additional parameters) value read from Chrome's ClientState key
    // during process startup.
    const wchar_t* update_ap;

    // The "name" value read from Chrome's ClientState\cohort key during process
    // startup.
    const wchar_t* update_cohort_name;

    // True if installed in C:\Program Files{, {x86)}; otherwise, false.
    bool system_level;

    // True if |channel| is an empty string for the extended stable channel.
    bool is_extended_stable_channel;
  };

  InstallDetails(const InstallDetails&) = delete;
  InstallDetails(InstallDetails&&) = delete;
  InstallDetails& operator=(const InstallDetails&) = delete;
  virtual ~InstallDetails() = default;

  // Returns the instance for this module.
  static const InstallDetails& Get();

  // This mode's index into the brand's array of install modes. This will match
  // a brand-specific InstallConstantIndex enumerator.
  int install_mode_index() const { return payload_->mode->index; }

  // Returns true if the current mode is the brand's primary install mode rather
  // than one of its secondary modes (e.g., canary Chrome).
  bool is_primary_mode() const { return install_mode_index() == 0; }

  // Returns the installer command-line switch that selects the current mode.
  const char* install_switch() const { return payload_->mode->install_switch; }

  // The mode's install suffix (e.g., " SxS" for canary Chrome), or an empty
  // string for a brand's primary install mode.
  const wchar_t* install_suffix() const {
    return payload_->mode->install_suffix;
  }

  // The mode's logo suffix (e.g., "Canary" for canary Chrome), or an empty
  // string for a brand's primary install mode.
  const wchar_t* logo_suffix() const { return payload_->mode->logo_suffix; }

  // Returns the full name of the installed product (e.g. "Chrome SxS" for
  // canary chrome).
  std::wstring install_full_name() const {
    return std::wstring(kProductPathName, kProductPathNameLength)
        .append(install_suffix());
  }

  const InstallConstants& mode() const { return *payload_->mode; }

  // The app GUID with which this mode is registered with Google Update, or an
  // empty string if this brand does not integrate with Google Update.
  const wchar_t* app_guid() const { return payload_->mode->app_guid; }

  // The toast activator CLSID with which Chrome is registered with the Windows
  // OS.
  const CLSID& toast_activator_clsid() const {
    return payload_->mode->toast_activator_clsid;
  }

  // The CLSID of the COM server that provides silent elevation functionality.
  const CLSID& elevator_clsid() const { return payload_->mode->elevator_clsid; }

  // The IID and the TypeLib of the IElevator interface that provides silent
  // elevation functionality.
  const IID& elevator_iid() const { return payload_->mode->elevator_iid; }

  // The CLSID of the COM server that provides ETW tracing functionality.
  const CLSID& tracing_service_clsid() const {
    return payload_->mode->tracing_service_clsid;
  }

  // The IID and the TypeLib of the ISystemTraceSession interface that provides
  // ETW tracing functionality.
  const IID& tracing_service_iid() const {
    return payload_->mode->tracing_service_iid;
  }

  // Returns the unsuffixed portion of the AppUserModelId. The AppUserModelId is
  // used to group an app's windows together on the Windows taskbar along with
  // its corresponding shortcuts; see
  // https://msdn.microsoft.com/library/windows/desktop/dd378459.aspx for more
  // information. Use ShellUtil::GetBrowserModelId to get the suffixed value --
  // it is almost never correct to use the unsuffixed (base) portion of this id
  // directly.
  const wchar_t* base_app_id() const { return payload_->mode->base_app_id; }

  // True if the mode supports installation at system-level.
  bool supports_system_level() const {
    return payload_->mode->supports_system_level;
  }

  // Returns the resource id of this mode's main application icon.
  int32_t app_icon_resource_id() const {
    return payload_->mode->app_icon_resource_id;
  }

  // The install's update channel, or an empty string if the brand does not
  // integrate with Google Update.
  std::wstring channel() const {
    return std::wstring(payload_->channel, payload_->channel_length);
  }

  // The origin of a ChannelStrategy::FLOATING install mode's channel, or
  // kInstallMode.
  ChannelOrigin channel_origin() const { return payload_->channel_origin; }

  // Returns the value that was used to select the channel if |channel_origin()|
  // returns kPolicy. This is the value provided to the installer via the
  // --channel= command line switch or the value provided to the browser via the
  // "channel" value in its Clients key.
  std::wstring channel_override() const {
    return payload_->channel_override ? std::wstring(payload_->channel_override)
                                      : std::wstring();
  }

  // Returns true if channel is an empty string for the extended stable channel.
  bool is_extended_stable_channel() const {
    return payload_->is_extended_stable_channel;
  }

  // Returns the "ap" (additional parameters) value read from Chrome's
  // ClientState key during process startup.
  std::wstring update_ap() const {
    return payload_->update_ap ? std::wstring(payload_->update_ap)
                               : std::wstring();
  }

  // Returns the "name" value read from Chrome's ClientState\cohort key during
  // process startup.
  std::wstring update_cohort_name() const {
    return payload_->update_cohort_name
               ? std::wstring(payload_->update_cohort_name)
               : std::wstring();
  }

  bool system_level() const { return payload_->system_level; }

  // Returns the path to the installation's ClientState registry key. This
  // registry key is used to hold various installation-related values, including
  // an indication of consent for usage stats.
  std::wstring GetClientStateKeyPath() const;

  // Returns the path to the installation's ClientStateMedium registry key. This
  // registry key is used to hold various installation-related values, including
  // an indication of consent for usage stats for a system-level install.
  std::wstring GetClientStateMediumKeyPath() const;

  // Returns true if there is an indication of a mismatch between the primary
  // module and this module.
  bool VersionMismatch() const;

  // Sets the instance for the process. This must be called only once per
  // process during startup.
  static void SetForProcess(std::unique_ptr<PrimaryInstallDetails> details);

  // Returns a pointer to the module's payload so that it may be shared with
  // other modules in the process.
  static const Payload* GetPayload();

  // Initializes this module's instance with the payload from the process's
  // primary module (the one that used SetForProcess).
  static void InitializeFromPayload(const Payload* payload);

 protected:
  explicit InstallDetails(const Payload* payload) : payload_(payload) {}
  const wchar_t* default_channel_name() const {
    return payload_->mode->default_channel_name;
  }

 private:
  friend class ScopedInstallDetails;

  // Swaps this module's instance with a provided instance, returning the
  // module's previous instance.
  static std::unique_ptr<const InstallDetails> Swap(
      std::unique_ptr<const InstallDetails> install_details);

  const Payload* const payload_;
};

// A kind of InstallDetails that owns its payload. A single instance of this
// class is initialized early on in process startup (e.g., in chrome_elf for the
// case of chrome.exe; see InitializeProductDetailsForPrimaryModule). Its
// underlying data (its "payload") is shared with other interested modules in
// the process.
class PrimaryInstallDetails : public InstallDetails {
 public:
  PrimaryInstallDetails();
  PrimaryInstallDetails(const PrimaryInstallDetails&) = delete;
  PrimaryInstallDetails(PrimaryInstallDetails&&) = delete;
  PrimaryInstallDetails& operator=(const PrimaryInstallDetails&) = delete;
  ~PrimaryInstallDetails() override;

  void set_mode(const InstallConstants* mode) { payload_.mode = mode; }
  void set_channel(const std::wstring& channel) {
    channel_ = channel;
    payload_.channel = channel_.c_str();
    payload_.channel_length = channel_.size();
  }
  void set_channel_origin(ChannelOrigin origin) {
    payload_.channel_origin = origin;
  }
  void set_channel_override(const std::wstring& channel_override) {
    channel_override_ = channel_override;
    payload_.channel_override = channel_override_.c_str();
  }
  void set_is_extended_stable_channel(bool is_extended_stable_channel) {
    payload_.is_extended_stable_channel = is_extended_stable_channel;
  }
  void set_update_ap(const std::wstring& update_ap) {
    update_ap_ = update_ap;
    payload_.update_ap = update_ap_.c_str();
  }
  void set_update_cohort_name(const std::wstring& update_cohort_name) {
    update_cohort_name_ = update_cohort_name;
    payload_.update_cohort_name = update_cohort_name_.c_str();
  }
  void set_system_level(bool system_level) {
    payload_.system_level = system_level;
  }

 private:
  std::wstring channel_;
  std::wstring channel_override_;
  std::wstring update_ap_;
  std::wstring update_cohort_name_;
  Payload payload_ = Payload();
};

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_INSTALL_DETAILS_H_
