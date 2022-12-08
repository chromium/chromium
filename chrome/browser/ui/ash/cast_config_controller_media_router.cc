// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/cast_config_controller_media_router.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/url_constants.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/browser/media_sinks_observer.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "components/user_manager/user_manager.h"

namespace {

absl::optional<media_router::MediaRouter*> media_router_for_test_;

Profile* GetProfile() {
  if (!user_manager::UserManager::IsInitialized())
    return nullptr;

  auto* user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return nullptr;

  return ash::ProfileHelper::Get()->GetProfileByUser(user);
}

// Returns the MediaRouter instance for the current primary profile, if there is
// one.
media_router::MediaRouter* GetMediaRouter() {
  if (media_router_for_test_)
    return *media_router_for_test_;

  Profile* profile = GetProfile();
  if (!profile || !media_router::MediaRouterEnabled(profile))
    return nullptr;

  auto* router =
      media_router::MediaRouterFactory::GetApiForBrowserContext(profile);
  DCHECK(router);
  return router;
}

// "Cast for Education" extension uses this string and expects the client to
// interpret it as "signed-in user's domain".
constexpr char const kDefaultDomain[] = "default";

}  // namespace

// This class caches the values that the observers give us so we can query them
// at any point in time. It also emits a device refresh event when new data is
// available.
class CastDeviceCache : public media_router::MediaRoutesObserver,
                        public media_router::MediaSinksObserver {
 public:
  using MediaSinks = std::vector<media_router::MediaSink>;
  using MediaRoutes = std::vector<media_router::MediaRoute>;
  using MediaRouteIds = std::vector<media_router::MediaRoute::Id>;

  explicit CastDeviceCache(
      const base::RepeatingClosure& update_devices_callback);

  CastDeviceCache(const CastDeviceCache&) = delete;
  CastDeviceCache& operator=(const CastDeviceCache&) = delete;

  ~CastDeviceCache() override;

  // This may run |update_devices_callback_| before returning.
  void Init();

  const MediaSinks& sinks() const { return sinks_; }
  const MediaRoutes& routes() const { return routes_; }

 private:
  // media_router::MediaSinksObserver:
  void OnSinksReceived(const MediaSinks& sinks) override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(const MediaRoutes& routes) override;

  MediaSinks sinks_;
  MediaRoutes routes_;

  base::RepeatingClosure update_devices_callback_;
};

CastDeviceCache::CastDeviceCache(
    const base::RepeatingClosure& update_devices_callback)
    : MediaRoutesObserver(GetMediaRouter()),
      MediaSinksObserver(GetMediaRouter(),
                         media_router::MediaSource::ForUnchosenDesktop(),
                         url::Origin()),
      update_devices_callback_(update_devices_callback) {}

CastDeviceCache::~CastDeviceCache() = default;

void CastDeviceCache::Init() {
  CHECK(MediaSinksObserver::Init());
}

void CastDeviceCache::OnSinksReceived(const MediaSinks& sinks) {
  sinks_.clear();
  for (const media_router::MediaSink& sink : sinks) {
    // The media router adds a MediaSink instance that doesn't have a name. Make
    // sure to filter that sink out from the UI so it is not rendered, as it
    // will be a line that only has a icon with no apparent meaning.
    if (sink.name().empty())
      continue;

    // Hide all sinks which have a non-default domain (ie, castouts) to meet
    // privacy requirements. This will be enabled once UI can display the
    // domain. See crbug.com/624016.
    if (sink.domain() && !sink.domain()->empty() &&
        sink.domain() != kDefaultDomain) {
      continue;
    }

    sinks_.push_back(sink);
  }

  update_devices_callback_.Run();
}

void CastDeviceCache::OnRoutesUpdated(const MediaRoutes& routes) {
  routes_ = routes;
  update_devices_callback_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// CastConfigControllerMediaRouter:

CastConfigControllerMediaRouter::CastConfigControllerMediaRouter() {
  // TODO(jdufault): This should use a callback interface once there is an
  // equivalent. See crbug.com/666005.
  session_observation_.Observe(session_manager::SessionManager::Get());
}

CastConfigControllerMediaRouter::~CastConfigControllerMediaRouter() = default;

// static
void CastConfigControllerMediaRouter::SetMediaRouterForTest(
    media_router::MediaRouter* media_router) {
  media_router_for_test_ = media_router;
}

CastDeviceCache* CastConfigControllerMediaRouter::device_cache() {
  // The CastDeviceCache instance is lazily allocated because the MediaRouter
  // component is not ready when the constructor is invoked.
  if (!device_cache_ && GetMediaRouter()) {
    device_cache_ = std::make_unique<CastDeviceCache>(base::BindRepeating(
        &CastConfigControllerMediaRouter::RequestDeviceRefresh,
        base::Unretained(this)));
    device_cache_->Init();
  }

  return device_cache_.get();
}

void CastConfigControllerMediaRouter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CastConfigControllerMediaRouter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool CastConfigControllerMediaRouter::HasMediaRouterForPrimaryProfile() const {
  return !!GetMediaRouter();
}

bool CastConfigControllerMediaRouter::HasSinksAndRoutes() const {
  return !devices_.empty();
}

bool CastConfigControllerMediaRouter::HasActiveRoute() const {
  for (const auto& device : devices_) {
    if (device.route.is_local_source && !device.route.title.empty())
      return true;
  }

  return false;
}

bool CastConfigControllerMediaRouter::AccessCodeCastingEnabled() const {
  Profile* profile = GetProfile();
  return base::FeatureList::IsEnabled(::features::kAccessCodeCastUI) &&
         profile && media_router::GetAccessCodeCastEnabledPref(profile);
}

void CastConfigControllerMediaRouter::RequestDeviceRefresh() {
  // The media router component isn't ready yet.
  if (!device_cache())
    return;

  // Build the old-style SinkAndRoute set out of the MediaRouter
  // source/sink/route setup. We first map the existing sinks, and then we
  // update those sinks with activity information.
  devices_.clear();

#if !defined(OFFICIAL_BUILD)
  // Optionally add fake cast devices for manual UI testing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kQsAddFakeCastDevices)) {
    AddFakeCastDevices();
  }
#endif

  for (const media_router::MediaSink& sink : device_cache()->sinks()) {
    ash::SinkAndRoute device;
    device.sink.id = sink.id();
    device.sink.name = sink.name();
    device.sink.domain = sink.domain().value_or(std::string());
    device.sink.sink_icon_type =
        static_cast<ash::SinkIconType>(sink.icon_type());
    devices_.push_back(std::move(device));
  }

  for (const media_router::MediaRoute& route : device_cache()->routes()) {
    for (ash::SinkAndRoute& device : devices_) {
      if (device.sink.id == route.media_sink_id()) {
        device.route.id = route.media_route_id();
        device.route.title = route.description();
        device.route.is_local_source = route.is_local();

        // Default to a tab/app capture. This will display the media router
        // description. This means we will properly support DIAL casts.
        device.route.content_source =
            route.media_source().IsDesktopMirroringSource()
                ? ash::ContentSource::kDesktop
                : ash::ContentSource::kTab;
        break;
      }
    }
  }

  for (auto& observer : observers_)
    observer.OnDevicesUpdated(devices_);
}

const std::vector<ash::SinkAndRoute>&
CastConfigControllerMediaRouter::GetSinksAndRoutes() {
  return devices_;
}

void CastConfigControllerMediaRouter::CastToSink(const std::string& sink_id) {
  if (GetMediaRouter()) {
    // TODO(imcheng): Pass in tab casting timeout.
    GetMediaRouter()->CreateRoute(
        media_router::MediaSource::ForUnchosenDesktop().id(), sink_id,
        url::Origin::Create(GURL("http://cros-cast-origin/")), nullptr,
        base::DoNothing(), base::TimeDelta(), false);
  }
}

void CastConfigControllerMediaRouter::StopCasting(const std::string& route_id) {
  if (GetMediaRouter())
    GetMediaRouter()->TerminateRoute(route_id);
}

void CastConfigControllerMediaRouter::OnUserProfileLoaded(
    const AccountId& account_id) {
  // The active profile has changed, which means that the media router has
  // as well. Reset the device cache to ensure we are using up-to-date
  // object instances.
  device_cache_.reset();
  RequestDeviceRefresh();
}

#if !defined(OFFICIAL_BUILD)
void CastConfigControllerMediaRouter::AddFakeCastDevices() {
  // Add enough devices that the UI menu will scroll.
  for (int i = 1; i <= 10; i++) {
    ash::SinkAndRoute device;
    device.sink.id = "fake_sink_id_" + base::NumberToString(i);
    device.sink.name = "Fake Sink " + base::NumberToString(i);
    device.sink.domain = "example.com";
    device.sink.sink_icon_type = ash::SinkIconType::kCast;
    devices_.push_back(std::move(device));
  }
}
#endif  // defined(OFFICIAL_BUILD)
