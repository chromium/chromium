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
#include "components/keyed_service/core/keyed_service.h"

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

  // Observer for the service notifications.
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever a skill is created, updated or deleted.
    virtual void OnSkillUpdated(std::string_view skill_id,
                                UpdateSource update_source) {}

    // Called when the service is ready to use and data is loaded from the disk.
    virtual void OnInitialized() {}
  };

  SkillsService();
  ~SkillsService() override;

  // Adds a new skill.
  // Generates a unique ID for the skill.
  // Returns a const pointer to the newly added skill.
  virtual const Skill* AddSkill(const std::string& name,
                                const std::string& icon,
                                const std::string& prompt) = 0;

  // Adds a new skill received from sync. Returns the newly created skill. The
  // difference from AddSkill is that this method takes a `skill_id` for the
  // created skill ID.
  virtual const Skill* AddSkillFromSync(std::string_view skill_id,
                                        std::string_view name,
                                        std::string_view icon,
                                        std::string_view prompt) = 0;

  // Updates an existing skill. Returns a skill if exists, nullptr otherwise.
  virtual const Skill* UpdateSkill(std::string_view skill_id,
                                   std::string_view name,
                                   std::string_view icon,
                                   std::string_view prompt,
                                   UpdateSource update_source) = 0;

  // Deletes a skill if exists.
  virtual void DeleteSkill(std::string_view skill_id,
                           UpdateSource update_source) = 0;

  // Returns the skill with the given ID or nullptr if not found.
  virtual const Skill* GetSkillById(std::string_view skill_id) const = 0;

  // Loads a skill list into memory.
  virtual void LoadInitialSkills(
      std::vector<std::unique_ptr<Skill>> initial_skills) = 0;

  // Returns a const reference to the currently loaded skills.
  virtual const std::vector<std::unique_ptr<Skill>>& GetSkills() const = 0;

  // Registers an observer for the service notifications.
  virtual void AddObserver(Observer* observer) = 0;

  // Unregisters an observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns controller delegate for the sync service.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate() = 0;
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_SERVICE_H_
