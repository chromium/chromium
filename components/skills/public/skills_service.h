// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_SERVICE_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/skills/internal/skills_downloader.h"
#include "components/skills/proto/skill.pb.h"
#include "components/skills/public/skills_types.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace skills {

struct Skill;

// Core service in charge of performing CRUD operations for skills. Each profile
// has one instance of this service.
class SkillsService : public KeyedService {
 public:
  // Source of the skill update.
  enum class UpdateSource {
    // The skill is updated locally, e.g. from the UI.
    kLocal,

    // The skill is updated by the sync service from the server.
    kSync,
  };

  enum class ServiceStatus {
    // The service is not initialized yet, i.e. LoadInitialSkills() has not
    // been called.
    kNotInitialized,

    // The service is initialized after browser startup but is not ready yet to
    // store data, i.e. sync is not ready. This is a transient state and
    // normally happens after sign-in for a short period of time while data is
    // being downloaded from the server.
    // This state is currently also used while signed out.
    kInitializedWaitingForSyncReady,

    // The service is initialized and ready to use, i.e. data is loaded from the
    // disk and sync is ready (initial download completed).
    kReady,
  };

  // Behavior of the skill display in the UI.
  enum class DisplayState {
    // When the skill is temporarily deleted in the UI.
    kDeleted,
    // When the skill is reshown in the UI if it was previously deleted from the
    // UI (e.g. when undo is pressed after a delete)
    kReshown,
  };

  // Observer for the service notifications.
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever a skill is created, updated or deleted.
    // `is_position_changed` is true if the skill's position is changed (always
    // false for deletions and true for creations).
    virtual void OnSkillUpdated(std::string_view skill_id,
                                UpdateSource update_source,
                                bool is_position_changed) {}

    // Called whenever a skill should be removed from the UI or brought back to
    // the UI via an undo.
    // NOTE: This will not actually delete the skill from the service, it will
    // just be hidden from the UI.
    virtual void OnTemporarySkillDisplay(
        std::string_view skill_id,
        SkillsService::DisplayState display_state) {}

    // Called when the service status is changed.
    virtual void OnStatusChanged() {}

    // Called when the service has completed a download of 1P skills. Receives
    // new data or nullptr if data has not changed.
    virtual void OnDiscoverySkillsUpdated(
        const FirstPartySkillData* first_party_skill_data) {}

    // Called when the service is shutting down. Observers should remove
    // themselves.
    virtual void OnSkillsServiceShuttingDown() {}

    // Returns true if the observer is active. This helps the skills service
    // determine if it needs to periodically refresh first party skills by
    // fetching them from the server.
    virtual bool Require1PSkillRefresh();
  };

  SkillsService();
  ~SkillsService() override;

  // Returns the service status.
  virtual ServiceStatus GetServiceStatus() const = 0;

  // Loads a skill list into memory from the disk and initializes the service.
  // Must be called only once.
  virtual void LoadInitialSkills(
      std::vector<std::unique_ptr<Skill>> initial_skills) = 0;

  // Adds a new skill locally.
  // Generates a unique ID for the skill.
  // Returns a const pointer to the newly added skill or nullptr in case of
  // failure (e.g. service is not in kReady state).
  virtual const Skill* AddSkill(const std::string& source_skill_id,
                                const std::string& name,
                                const std::string& icon,
                                const std::string& prompt) = 0;

  // Adds a new or updates an existing skill received from sync. Returns the
  // newly created or updated skill. The difference from AddSkill() is that this
  // method takes a `skill_id` for the created skill ID. Must only be called
  // when the service is in kReady state.
  virtual const Skill* AddOrUpdateSkillFromSync(
      std::string_view skill_id,
      std::string_view source_skill_id,
      std::string_view name,
      std::string_view icon,
      std::string_view prompt,
      std::string_view description,
      base::Time creation_time,
      base::Time last_update_time,
      sync_pb::SkillSource source) = 0;

  // Updates an existing skill locally. Returns a skill if exists, nullptr
  // otherwise.
  virtual const Skill* UpdateSkill(std::string_view skill_id,
                                   std::string_view name,
                                   std::string_view icon,
                                   std::string_view prompt) = 0;

  // Deletes a skill if exists (locally or from sync).
  virtual void DeleteSkill(std::string_view skill_id,
                           UpdateSource update_source) = 0;

  // Returns the skill with the given ID or nullptr if not found (including
  // when the service is not in kReady state).
  virtual const Skill* GetSkillById(std::string_view skill_id) const = 0;

  // Returns a const reference to the currently loaded skills. Returns an empty
  // list if the service is not in kReady state.
  virtual const std::vector<std::unique_ptr<Skill>>& GetSkills() const = 0;

  // Returns a const reference to the currently loaded 1p skills. If skills have
  // not been loaded yet, returns an empty map. The service does not have to be
  // in a kReady state since these skills are loaded from a SCS file.
  virtual const SkillIdToProtoMap& Get1PSkills() const = 0;

  // Returns a const reference to the currently loaded 1p topics.
  virtual const std::vector<std::string>& Get1PTopics() const = 0;

  // Registers an observer for the service notifications.
  virtual void AddObserver(Observer* observer) = 0;

  // Unregisters an observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Refreshes the list of discovery skills.
  // 1st party discovery skills need to be refreshed periodically, to ensure
  // that any security updates are applied, and that users do not invoke a skill
  // that has been taken down.
  virtual void RefreshDiscoverySkills() = 0;

  // Calls downloader to fetch 1p skills which will return updated skills to
  // Handle1pSkillsMap. If there has been no modification since the last fetch
  // nullptr will be returned.
  virtual void FetchDiscoverySkills() = 0;

  // Called on download complete of 1p skills. If the download fails or the file
  // has not been modified first_party_skill_data is null. Notifies observers.
  virtual void Handle1pSkillsMap(
      std::unique_ptr<FirstPartySkillData> first_party_skill_data) = 0;

  // Returns controller delegate for the sync service.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate() = 0;

  // Called when the sync bridge status is changed.
  virtual void SyncStatusChanged() = 0;

  // Sets the service status for testing purposes. This is useful for testing in
  // browser tests where the sync server is not available.
  virtual void SetServiceStatusForTesting(ServiceStatus status) = 0;

  // Called when a skill should be removed from the UI or brought back to the UI
  // via an undo.
  // NOTE: This will not actually delete the skill from the
  // service, it will just be hidden from the UI.
  virtual void NotifyTemporarySkillDisplayChanged(
      std::string_view skill_id,
      DisplayState display_state) = 0;

  // Notify that a glic panel associated with the skills service is being
  // opened.
  virtual void NotifyPanelWillOpen() = 0;
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_SERVICE_H_
