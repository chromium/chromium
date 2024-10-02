// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_OBJECT_PERMISSION_CONTEXT_BASE_H_
#define COMPONENTS_PERMISSIONS_OBJECT_PERMISSION_CONTEXT_BASE_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class HostContentSettingsMap;

namespace url {
class Origin;
}

namespace permissions {

// This is the base class for services that manage any type of permission that
// is associated with a more complicated grant than simple allow/deny. This is
// typically granted through a chooser-style UI.
// Subclasses must define the structure of the objects that are stored.
class ObjectPermissionContextBase : public KeyedService {
 public:
  struct Object {
    Object(const url::Origin& origin,
           base::Value::Dict value,
           content_settings::SettingSource source,
           bool incognito);
    // DEPRECATED.
    // TODO(crbug.com/40172729): Migrate value to base::Value::Dict.
    Object(const url::Origin& origin,
           base::Value value,
           content_settings::SettingSource source,
           bool incognito);
    ~Object();
    std::unique_ptr<Object> Clone();

    GURL origin;
    base::Value::Dict value;
    content_settings::SettingSource source;
    bool incognito;
  };

  using ObjectMap =
      std::map<url::Origin, std::map<std::string, std::unique_ptr<Object>>>;

  // This observer can be used to be notified of changes to the permission of
  // an object.
  class PermissionObserver : public base::CheckedObserver {
   public:
    // Notify observer that an object permission changed for the permission
    // context represented by |guard_content_settings_type|, if applicable, and
    // |data_content_settings_type|.
    virtual void OnObjectPermissionChanged(
        std::optional<ContentSettingsType> guard_content_settings_type,
        ContentSettingsType data_content_settings_type);
    // Notify observer that an object permission was revoked for |origin|.
    virtual void OnPermissionRevoked(const url::Origin& origin);
  };

  void AddObserver(PermissionObserver* observer);
  void RemoveObserver(PermissionObserver* observer);

  ObjectPermissionContextBase(
      ContentSettingsType guard_content_settings_type,
      ContentSettingsType data_content_settings_type,
      HostContentSettingsMap* host_content_settings_map);
  ObjectPermissionContextBase(
      ContentSettingsType data_content_settings_type,
      HostContentSettingsMap* host_content_settings_map);
  ~ObjectPermissionContextBase() override;

  // Checks whether |origin| can request permission to access objects. This is
  // done by checking |guard_content_settings_type_| is in the "ask" state.
  bool CanRequestObjectPermission(const url::Origin& origin);

  // Returns the object corresponding to |key| that |origin| has been granted
  // permission to access. This method should only be called if
  // |GetKeyForObject()| is overridden to return sensible keys.
  //
  // This method may be extended by a subclass to return
  // objects not stored in |host_content_settings_map_|.
  virtual std::unique_ptr<Object> GetGrantedObject(const url::Origin& origin,
                                                   std::string_view key);

  // Returns the list of objects that |origin| has been granted permission to
  // access. This method may be extended by a subclass to return objects not
  // stored in |host_content_settings_map_|.
  virtual std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& origin);

  // Returns a set of all origins that have granted permission(s).
  // This method may be extended by a subclass to return origins with objects
  // not stored in |host_content_settings_map_|.
  virtual std::set<url::Origin> GetOriginsWithGrants();

  // Returns the set of all objects that any origin has been granted permission
  // to access.
  //
  // This method may be extended by a subclass to return objects not stored in
  // |host_content_settings_map_|.
  virtual std::vector<std::unique_ptr<Object>> GetAllGrantedObjects();

  // Grants |origin| access to |object| by writing it into
  // |host_content_settings_map_|.
  // TODO(crbug.com/40755589): Combine GrantObjectPermission and
  // UpdateObjectPermission methods into key-based GrantOrUpdateObjectPermission
  // once backend is updated to make key-based methods more efficient.
  void GrantObjectPermission(const url::Origin& origin,
                             base::Value::Dict object);

  // Updates |old_object| with |new_object| for |origin|, and writes the value
  // into |host_content_settings_map_|.
  void UpdateObjectPermission(const url::Origin& origin,
                              const base::Value::Dict& old_object,
                              base::Value::Dict new_object);

  // Revokes |origin|'s permission to access |object|.
  //
  // This method may be extended by a subclass to revoke permission to access
  // objects returned by GetGrantedObjects but not stored in
  // |host_content_settings_map_|.
  // TODO(crbug.com/40755589): Remove this method once backend is updated
  // to make key-based methods more efficient.
  virtual void RevokeObjectPermission(const url::Origin& origin,
                                      const base::Value::Dict& object);

  // Revokes |origin|'s permission to access the object corresponding to |key|.
  // This method should only be called if |GetKeyForObject()| is overridden to
  // return sensible keys.
  //
  // This method may be extended by a subclass to revoke permission to access
  // objects returned by GetGrantedObjects but not stored in
  // |host_content_settings_map_|.
  virtual void RevokeObjectPermission(const url::Origin& origin,
                                      std::string_view key);

  // Revokes a given `origin`'s permissions for access to all of its
  // corresponding objects.
  //
  // This method may be extended by a subclass to revoke permissions to access
  // objects returned by `GetGrantedObjects` but not stored in the
  // `host_content_settings_map`.
  virtual bool RevokeObjectPermissions(const url::Origin& origin);

  // Returns a string which is used to uniquely identify this object.
  virtual std::string GetKeyForObject(const base::Value::Dict& object) = 0;

  // Validates the structure of an object read from
  // |host_content_settings_map_|.
  virtual bool IsValidObject(const base::Value::Dict& object) = 0;

  // Gets the human-readable name for a given object.
  virtual std::u16string GetObjectDisplayName(
      const base::Value::Dict& object) = 0;

  // Triggers the immediate flushing of all scheduled save setting operations.
  // To be called when the host_content_settings_map_ is about to become
  // unusable (e.g. browser context shutting down).
  void FlushScheduledSaveSettingsCalls();

 protected:
  // TODO(odejesush): Use this method in all derived classes instead of using a
  // member variable to store this state.
  bool IsOffTheRecord();
  void NotifyPermissionChanged();
  void NotifyPermissionRevoked(const url::Origin& origin);

  const std::optional<ContentSettingsType> guard_content_settings_type_;
  const ContentSettingsType data_content_settings_type_;
  base::ObserverList<PermissionObserver> permission_observer_list_;

 private:
  base::Value::Dict GetWebsiteSetting(const url::Origin& origin,
                                      content_settings::SettingInfo* info);
  void SaveWebsiteSetting(const url::Origin& origin);
  void ScheduleSaveWebsiteSetting(const url::Origin& origin);
  virtual std::vector<std::unique_ptr<Object>> GetWebsiteSettingObjects();
  void LoadWebsiteSettingsIntoObjects();

  // Getter for `objects_` used to initialize the structure at first access.
  // Never use the `objects_` member directly outside of this function.
  ObjectMap& objects();

  const raw_ptr<HostContentSettingsMap, DanglingUntriaged>
      host_content_settings_map_;

  // In-memory cache that holds the granted objects. Lazy-initialized by first
  // call to `objects()`.
  ObjectMap objects_;

  // Whether the `objects_` member was initialized;
  bool objects_initialized_ = false;

  // Origins that have a scheduled `SaveWebsiteSetting` call.
  base::flat_set<url::Origin> origins_with_scheduled_save_settings_calls_;

  base::WeakPtrFactory<ObjectPermissionContextBase> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_OBJECT_PERMISSION_CONTEXT_BASE_H_
