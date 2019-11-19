// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/cast_config_controller_media_router.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/url_constants.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace {

base::Optional<media_router::MediaRouter*> media_router_for_test_;

// Returns the MediaRouter instance for the current primary profile, if there is
// one.
media_router::MediaRouter* GetMediaRouter() {
  if (media_router_for_test_)
    return *media_router_for_test_;

  if (!user_manager::UserManager::IsInitialized())
    return nullptr;

  auto* user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return nullptr;

  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
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
  ~CastDeviceCache() override;

  // This may run |update_devices_callback_| before returning.
  void Init();

  const MediaSinks& sinks() const { return sinks_; }
  const MediaRoutes& routes() const { return routes_; }

 private:
  // media_router::MediaSinksObserver:
  void OnSinksReceived(const MediaSinks& sinks) override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(const MediaRoutes& routes,
                       const MediaRouteIds& unused_joinable_route_ids) override;

  MediaSinks sinks_;
  MediaRoutes routes_;

  base::RepeatingClosure update_devices_callback_;

  DISALLOW_COPY_AND_ASSIGN(CastDeviceCache);
};

CastDeviceCache::CastDeviceCache(
    const base::RepeatingClosure& update_devices_callback)
    : MediaRoutesObserver(GetMediaRouter()),
      MediaSinksObserver(GetMediaRouter(),
                         media_router::MediaSource::ForDesktop(),
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

void CastDeviceCache::OnRoutesUpdated(
    const MediaRoutes& routes,
    const MediaRouteIds& unused_joinable_route_ids) {
  routes_ = routes;
  update_devices_callback_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// CastConfigControllerMediaRouter:

void CastConfigControllerMediaRouter::SetMediaRouterForTest(
    media_router::MediaRouter* media_router) {
  media_router_for_test_ = media_router;
}

CastConfigControllerMediaRouter::CastConfigControllerMediaRouter() {
  // TODO(jdufault): This should use a callback interface once there is an
  // equivalent. See crbug.com/666005.
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());
}

CastConfigControllerMediaRouter::~CastConfigControllerMediaRouter() = default;

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

void CastConfigControllerMediaRouter::RequestDeviceRefresh() {
  // The media router component isn't ready yet.
  if (!device_cache())
    return;

  // Build the old-style SinkAndRoute set out of the MediaRouter
  // source/sink/route setup. We first map the existing sinks, and then we
  // update those sinks with activity information.
  devices_.clear();

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
    if (!route.for_display())
      continue;

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
  // TODO(imcheng): Pass in tab casting timeout.
  GetMediaRouter()->CreateRoute(
      media_router::MediaSource::ForDesktop().id(), sink_id,
      url::Origin::Create(GURL("http://cros-cast-origin/")), nullptr,
      base::DoNothing(), base::TimeDelta(), false);
}

void CastConfigControllerMediaRouter::StopCasting(const std::string& route_id) {
  GetMediaRouter()->TerminateRoute(route_id);
}

void CastConfigControllerMediaRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED:
      // The active profile has changed, which means that the media router has
      // as well. Reset the device cache to ensure we are using up-to-date
      // object instances.
      device_cache_.reset();
      RequestDeviceRefresh();
      break;
  }
}
