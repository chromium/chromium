// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Generating a fingerprint consists of two major steps:
//   (1) Gather all the necessary data.
//   (2) Write it into a protocol buffer.
//
// Step (2) is as simple as it sounds -- it's really just a matter of copying
// data.  Step (1) requires waiting on several asynchronous callbacks, which are
// managed by the FingerprintDataLoader class.

#include "components/autofill/content/browser/risk/fingerprint.h"

#include <utility>

#include "base/check.h"
#include "base/cpu.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "components/autofill/content/common/content_autofill_features.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/webplugininfo.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "ui/display/display.h"
#include "ui/display/display_util.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace autofill {
namespace risk {

namespace {

const int32_t kFingerprinterVersion = 1;

// Maximum amount of time, in seconds, to wait for loading asynchronous
// fingerprint data.
const int kTimeoutSeconds = 4;

// Returns the delta between the local timezone and UTC.
base::TimeDelta GetTimezoneOffset() {
  const base::Time utc = AutofillClock::Now();

  base::Time::Exploded local;
  utc.LocalExplode(&local);

  base::Time out_time;
  bool conversion_success = base::Time::FromUTCExploded(local, &out_time);
  DCHECK(conversion_success);

  return out_time - utc;
}

// Returns the concatenation of the operating system name and version, e.g.
// "Mac OS X 10.6.8".
std::string GetOperatingSystemVersion() {
  return base::SysInfo::OperatingSystemName() + " " +
         base::SysInfo::OperatingSystemVersion();
}

// Adds the list of |fonts| to the |machine|.
void AddFontsToFingerprint(const base::Value::List& fonts,
                           Fingerprint::MachineCharacteristics* machine) {
  for (const auto& it : fonts) {
    // Each item in the list is a two-element list such that the first element
    // is the font family and the second is the font name.
    DCHECK(it.is_list());

    std::string font_name = it.GetList()[1].GetString();

    machine->add_font(font_name);
  }
}

// Adds the list of |plugins| to the |machine|.
void AddPluginsToFingerprint(const std::vector<content::WebPluginInfo>& plugins,
                             Fingerprint::MachineCharacteristics* machine) {
  for (const content::WebPluginInfo& it : plugins) {
    Fingerprint::MachineCharacteristics::Plugin* plugin = machine->add_plugin();
    plugin->set_name(base::UTF16ToUTF8(it.name));
    plugin->set_description(base::UTF16ToUTF8(it.desc));
    for (const content::WebPluginMimeType& mime_type : it.mime_types)
      plugin->add_mime_type(mime_type.mime_type);
    plugin->set_version(base::UTF16ToUTF8(it.version));
  }
}

// Adds the list of HTTP accept languages to the |machine|.
void AddAcceptLanguagesToFingerprint(
    const std::string& accept_languages_str,
    Fingerprint::MachineCharacteristics* machine) {
  for (const std::string& lang :
       base::SplitString(accept_languages_str, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_ALL))
    machine->add_requested_language(lang);
}

// This function writes
//   (a) the number of screens,
//   (b) the primary display's screen size,
//   (c) the screen's color depth, and
//   (d) the size of the screen unavailable to web page content,
//       i.e. the Taskbar size on Windows
// into the |machine|.
void AddScreenInfoToFingerprint(const display::ScreenInfo& screen_info,
                                Fingerprint::MachineCharacteristics* machine) {
  machine->set_screen_count(display::Screen::GetScreen()->GetNumDisplays());

  const gfx::Size screen_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
  machine->mutable_screen_size()->set_width(screen_size.width());
  machine->mutable_screen_size()->set_height(screen_size.height());

  machine->set_screen_color_depth(screen_info.depth);

  const gfx::Rect screen_rect(screen_info.rect);
  const gfx::Rect available_rect(screen_info.available_rect);
  const gfx::Rect unavailable_rect =
      gfx::SubtractRects(screen_rect, available_rect);
  machine->mutable_unavailable_screen_size()->set_width(
      unavailable_rect.width());
  machine->mutable_unavailable_screen_size()->set_height(
      unavailable_rect.height());
}

// Writes info about the machine's CPU into the |machine|.
void AddCpuInfoToFingerprint(Fingerprint::MachineCharacteristics* machine) {
  base::CPU cpu;
  machine->mutable_cpu()->set_vendor_name(cpu.vendor_name());
  machine->mutable_cpu()->set_brand(cpu.cpu_brand());
}

// Writes info about the machine's GPU into the |machine|.
void AddGpuInfoToFingerprint(Fingerprint::MachineCharacteristics* machine,
                             content::GpuDataManager& gpu_data_manager) {
  if (!gpu_data_manager.IsEssentialGpuInfoAvailable())
    return;

  const gpu::GPUInfo gpu_info = gpu_data_manager.GetGPUInfo();
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();

  Fingerprint::MachineCharacteristics::Graphics* graphics =
      machine->mutable_graphics_card();
  graphics->set_vendor_id(active_gpu.vendor_id);
  graphics->set_device_id(active_gpu.device_id);
  graphics->set_driver_version(active_gpu.driver_version);
}

// Waits for all asynchronous data required for the fingerprint to be loaded,
// then fills out the fingerprint.
class FingerprintDataLoader : public content::GpuDataManagerObserver {
 public:
  FingerprintDataLoader(
      uint64_t obfuscated_gaia_id,
      const gfx::Rect& window_bounds,
      const gfx::Rect& content_bounds,
      const display::ScreenInfo& screen_info,
      const std::string& version,
      const std::string& charset,
      const std::string& accept_languages,
      base::Time install_time,
      const std::string& app_locale,
      const std::string& user_agent,
      base::TimeDelta timeout,
      base::OnceCallback<void(std::unique_ptr<Fingerprint>)> callback);

  FingerprintDataLoader(const FingerprintDataLoader&) = delete;
  FingerprintDataLoader& operator=(const FingerprintDataLoader&) = delete;

 private:
  ~FingerprintDataLoader() override = default;

  // content::GpuDataManagerObserver:
  void OnGpuInfoUpdate() override;

  // Callbacks for asynchronously loaded data.
  void OnGotFonts(base::Value::List fonts);
  void OnGotPlugins(const std::vector<content::WebPluginInfo>& plugins);
  void OnGotGeoposition(device::mojom::GeopositionResultPtr result);

  // If all of the asynchronous data has been loaded, calls |callback_| with
  // the fingerprint data.
  void MaybeFillFingerprint();

  // Calls |callback_| with the fingerprint data.
  void FillFingerprint();

  // The GPU data provider.
  // Weak reference because the GpuDataManager class is a singleton.
  const raw_ptr<content::GpuDataManager> gpu_data_manager_;

  // Ensures that any observer registrations for the GPU data are cleaned up by
  // the time this object is destroyed.
  base::ScopedObservation<content::GpuDataManager,
                          content::GpuDataManagerObserver>
      gpu_observation_{this};

  // Data that will be passed on to the next loading phase.  See the comment for
  // GetFingerprint() for a description of these variables.
  const uint64_t obfuscated_gaia_id_;
  const gfx::Rect window_bounds_;
  const gfx::Rect content_bounds_;
  const display::ScreenInfo screen_info_;
  const std::string version_;
  const std::string charset_;
  const std::string accept_languages_;
  const std::string app_locale_;
  const std::string user_agent_;
  const base::Time install_time_;

  // Data that will be loaded asynchronously.
  std::unique_ptr<base::Value::List> fonts_;
  std::vector<content::WebPluginInfo> plugins_;
  bool waiting_on_plugins_ = true;
  device::mojom::GeopositionResultPtr geoposition_result_;
  mojo::Remote<device::mojom::Geolocation> geolocation_;
  mojo::Remote<device::mojom::GeolocationContext> geolocation_context_;

  // Timer to enforce a maximum timeout before the |callback_| is called, even
  // if not all asynchronous data has been loaded.
  base::OneShotTimer timeout_timer_;

  // The callback that will be called once all the data is available.
  base::OnceCallback<void(std::unique_ptr<Fingerprint>)> callback_;

  // For invalidating asynchronous callbacks that might arrive after |this|
  // instance is destroyed.
  base::WeakPtrFactory<FingerprintDataLoader> weak_ptr_factory_{this};
};

FingerprintDataLoader::FingerprintDataLoader(
    uint64_t obfuscated_gaia_id,
    const gfx::Rect& window_bounds,
    const gfx::Rect& content_bounds,
    const display::ScreenInfo& screen_info,
    const std::string& version,
    const std::string& charset,
    const std::string& accept_languages,
    base::Time install_time,
    const std::string& app_locale,
    const std::string& user_agent,
    base::TimeDelta timeout,
    base::OnceCallback<void(std::unique_ptr<Fingerprint>)> callback)
    : gpu_data_manager_(content::GpuDataManager::GetInstance()),
      obfuscated_gaia_id_(obfuscated_gaia_id),
      window_bounds_(window_bounds),
      content_bounds_(content_bounds),
      screen_info_(screen_info),
      version_(version),
      charset_(charset),
      accept_languages_(accept_languages),
      app_locale_(app_locale),
      user_agent_(user_agent),
      install_time_(install_time),
      callback_(std::move(callback)) {
  DCHECK(!install_time_.is_null());

  timeout_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&FingerprintDataLoader::MaybeFillFingerprint,
                     weak_ptr_factory_.GetWeakPtr()));

  // Load GPU data if needed.
  if (gpu_data_manager_->GpuAccessAllowed(nullptr) &&
      !gpu_data_manager_->IsEssentialGpuInfoAvailable()) {
    gpu_observation_.Observe(gpu_data_manager_.get());
    OnGpuInfoUpdate();
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  // Load plugin data.
  content::PluginService::GetInstance()->GetPlugins(base::BindOnce(
      &FingerprintDataLoader::OnGotPlugins, weak_ptr_factory_.GetWeakPtr()));
#else
  waiting_on_plugins_ = false;
#endif

  // Load font data.
  content::GetFontListAsync(base::BindOnce(&FingerprintDataLoader::OnGotFonts,
                                           weak_ptr_factory_.GetWeakPtr()));

  if (!base::FeatureList::IsEnabled(
          features::kAutofillDisableGeolocationInRiskFingerprint)) {
    // Load geolocation data.
    content::GetDeviceService().BindGeolocationContext(
        geolocation_context_.BindNewPipeAndPassReceiver());
    geolocation_context_->BindGeolocation(
        geolocation_.BindNewPipeAndPassReceiver(), GURL(),
        device::mojom::GeolocationClientId::kFingerprintDataLoader);
    geolocation_->SetHighAccuracy(false);
    geolocation_->QueryNextPosition(
        base::BindOnce(&FingerprintDataLoader::OnGotGeoposition,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void FingerprintDataLoader::OnGpuInfoUpdate() {
  if (!gpu_data_manager_->IsEssentialGpuInfoAvailable())
    return;

  DCHECK(gpu_observation_.IsObservingSource(gpu_data_manager_.get()));
  gpu_observation_.Reset();
  MaybeFillFingerprint();
}

void FingerprintDataLoader::OnGotFonts(base::Value::List fonts) {
  DCHECK(!fonts_);
  fonts_ = std::make_unique<base::Value::List>(std::move(fonts));
  MaybeFillFingerprint();
}

void FingerprintDataLoader::OnGotPlugins(
    const std::vector<content::WebPluginInfo>& plugins) {
  DCHECK(waiting_on_plugins_);
  waiting_on_plugins_ = false;
  plugins_ = plugins;
  MaybeFillFingerprint();
}

void FingerprintDataLoader::OnGotGeoposition(
    device::mojom::GeopositionResultPtr result) {
  DCHECK(!geoposition_result_);
  DCHECK(result);
  geoposition_result_ = std::move(result);

  geolocation_.reset();
  geolocation_context_.reset();

  MaybeFillFingerprint();
}

void FingerprintDataLoader::MaybeFillFingerprint() {
  // If all of the data has been loaded, or if the |timeout_timer_| has expired,
  // fill the fingerprint and clean up.
  const bool requires_geoposition_result = !base::FeatureList::IsEnabled(
      features::kAutofillDisableGeolocationInRiskFingerprint);
  if (!timeout_timer_.IsRunning() ||
      ((!gpu_data_manager_->GpuAccessAllowed(nullptr) ||
        gpu_data_manager_->IsEssentialGpuInfoAvailable()) &&
       fonts_ && !waiting_on_plugins_ &&
       (geoposition_result_ || !requires_geoposition_result))) {
    FillFingerprint();
    delete this;
  }
}

void FingerprintDataLoader::FillFingerprint() {
  std::unique_ptr<Fingerprint> fingerprint(new Fingerprint);
  Fingerprint::MachineCharacteristics* machine =
      fingerprint->mutable_machine_characteristics();

  machine->set_operating_system_build(GetOperatingSystemVersion());
  // We use the delta between the install time and the Unix epoch, in hours.
  machine->set_browser_install_time_hours(
      (install_time_ - base::Time::UnixEpoch()).InHours());
  machine->set_utc_offset_ms(GetTimezoneOffset().InMilliseconds());
  machine->set_browser_language(app_locale_);
  machine->set_charset(charset_);
  machine->set_user_agent(user_agent_);
  machine->set_ram(base::SysInfo::AmountOfPhysicalMemory());
  machine->set_browser_build(version_);
  machine->set_browser_feature(
      Fingerprint::MachineCharacteristics::FEATURE_REQUEST_AUTOCOMPLETE);
  if (fonts_)
    AddFontsToFingerprint(*fonts_, machine);
  AddPluginsToFingerprint(plugins_, machine);
  AddAcceptLanguagesToFingerprint(accept_languages_, machine);
  AddScreenInfoToFingerprint(screen_info_, machine);
  AddCpuInfoToFingerprint(machine);
  AddGpuInfoToFingerprint(machine, *gpu_data_manager_);

  // TODO(isherman): Record the user_and_device_name_hash.
  // TODO(isherman): Record the partition size of the hard drives?

  Fingerprint::TransientState* transient_state =
      fingerprint->mutable_transient_state();
  Fingerprint::Dimension* inner_window_size =
      transient_state->mutable_inner_window_size();
  inner_window_size->set_width(content_bounds_.width());
  inner_window_size->set_height(content_bounds_.height());
  Fingerprint::Dimension* outer_window_size =
      transient_state->mutable_outer_window_size();
  outer_window_size->set_width(window_bounds_.width());
  outer_window_size->set_height(window_bounds_.height());

  // TODO(isherman): Record network performance data, which is theoretically
  // available to JS.

  // TODO(isherman): Record more user behavior data.
  if (geoposition_result_ && geoposition_result_->is_position()) {
    const auto& geoposition = *geoposition_result_->get_position();
    Fingerprint::UserCharacteristics::Location* location =
        fingerprint->mutable_user_characteristics()->mutable_location();
    location->set_altitude(geoposition.altitude);
    location->set_latitude(geoposition.latitude);
    location->set_longitude(geoposition.longitude);
    location->set_accuracy(geoposition.accuracy);
    location->set_time_in_ms(
        (geoposition.timestamp - base::Time::UnixEpoch()).InMilliseconds());
  }

  Fingerprint::Metadata* metadata = fingerprint->mutable_metadata();
  metadata->set_timestamp_ms(
      (AutofillClock::Now() - base::Time::UnixEpoch()).InMilliseconds());
  metadata->set_obfuscated_gaia_id(obfuscated_gaia_id_);
  metadata->set_fingerprinter_version(kFingerprinterVersion);

  std::move(callback_).Run(std::move(fingerprint));
}

}  // namespace

namespace internal {

void GetFingerprintInternal(
    uint64_t obfuscated_gaia_id,
    const gfx::Rect& window_bounds,
    const gfx::Rect& content_bounds,
    const display::ScreenInfo& screen_info,
    const std::string& version,
    const std::string& charset,
    const std::string& accept_languages,
    base::Time install_time,
    const std::string& app_locale,
    const std::string& user_agent,
    base::TimeDelta timeout,
    base::OnceCallback<void(std::unique_ptr<Fingerprint>)> callback) {
  // Begin loading all of the data that we need to load asynchronously.
  // This class is responsible for freeing its own memory.
  new FingerprintDataLoader(obfuscated_gaia_id, window_bounds, content_bounds,
                            screen_info, version, charset, accept_languages,
                            install_time, app_locale, user_agent, timeout,
                            std::move(callback));
}

}  // namespace internal

void GetFingerprint(
    uint64_t obfuscated_gaia_id,
    const gfx::Rect& window_bounds,
    content::WebContents* web_contents,
    const std::string& version,
    const std::string& charset,
    const std::string& accept_languages,
    base::Time install_time,
    const std::string& app_locale,
    const std::string& user_agent,
    base::OnceCallback<void(std::unique_ptr<Fingerprint>)> callback) {
  gfx::Rect content_bounds;
  display::ScreenInfo screen_info;

  // |web_contents| can be nullptr in the Clank settings page, as a user can
  // open Clank settings without opening a tab. Thus, we will need to populate
  // |screen_info| using display::DisplayUtil::GetDefaultScreenInfo().
  if (web_contents) {
    content_bounds = web_contents->GetContainerBounds();
    raw_ptr<content::RenderWidgetHostView> host_view =
        web_contents->GetRenderWidgetHostView();
    if (host_view)
      screen_info = host_view->GetRenderWidgetHost()->GetScreenInfo();
  } else {
    display::DisplayUtil::GetDefaultScreenInfo(&screen_info);
  }

  internal::GetFingerprintInternal(
      obfuscated_gaia_id, window_bounds, content_bounds, screen_info, version,
      charset, accept_languages, install_time, app_locale, user_agent,
      base::Seconds(kTimeoutSeconds), std::move(callback));
}

}  // namespace risk
}  // namespace autofill
