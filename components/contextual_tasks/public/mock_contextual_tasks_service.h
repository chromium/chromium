// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_MOCK_CONTEXTUAL_TASKS_SERVICE_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_MOCK_CONTEXTUAL_TASKS_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/uuid.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace contextual_tasks {

class MockContextualTasksService : public ContextualTasksService {
 public:
  MockContextualTasksService();
  ~MockContextualTasksService() override;

  MOCK_METHOD(std::optional<ContextualTask>,
              GetContextualTaskForTab,
              (SessionID),
              (const, override));
  MOCK_METHOD(
      void,
      GetContextForTask,
      (const base::Uuid&,
       const std::set<ContextualTaskContextSource>&,
       std::unique_ptr<ContextDecorationParams>,
       base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>),
      (override));
  MOCK_METHOD(void,
              AddObserver,
              (ContextualTasksService::Observer*),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (ContextualTasksService::Observer*),
              (override));
  MOCK_METHOD(FeatureEligibility, GetFeatureEligibility, (), (override));
  MOCK_METHOD(bool, IsInitialized, (), (override));
  MOCK_METHOD(ContextualTask, CreateTask, (), (override));
  MOCK_METHOD(ContextualTask, CreateTaskFromUrl, (const GURL&), (override));
  MOCK_METHOD(void,
              GetTaskById,
              (const base::Uuid&,
               base::OnceCallback<void(std::optional<ContextualTask>)>),
              (const, override));
  MOCK_METHOD(void,
              GetTasks,
              (base::OnceCallback<void(std::vector<ContextualTask>)>),
              (const, override));
  MOCK_METHOD(void, DeleteTask, (const base::Uuid&), (override));
  MOCK_METHOD(void,
              UpdateThreadForTask,
              (const base::Uuid&,
               ThreadType,
               const std::string&,
               std::optional<std::string>,
               std::optional<std::string>),
              (override));
  MOCK_METHOD(void,
              RemoveThreadFromTask,
              (const base::Uuid&, ThreadType, const std::string&),
              (override));
  MOCK_METHOD(std::optional<ContextualTask>,
              GetTaskFromServerId,
              (ThreadType, const std::string&),
              (override));
  MOCK_METHOD(void,
              AttachUrlToTask,
              (const base::Uuid&, const GURL&),
              (override));
  MOCK_METHOD(void,
              DetachUrlFromTask,
              (const base::Uuid&, const GURL&),
              (override));
  MOCK_METHOD(void,
              SetUrlResourcesFromServer,
              (const base::Uuid& task_id,
               std::vector<UrlResource> url_resources),
              (override));
  MOCK_METHOD(void,
              AssociateTabWithTask,
              (const base::Uuid&, SessionID),
              (override));
  MOCK_METHOD(void,
              DisassociateTabFromTask,
              (const base::Uuid&, SessionID),
              (override));
  MOCK_METHOD(void,
              DisassociateAllTabsFromTask,
              (const base::Uuid& task_id),
              (override));
  MOCK_METHOD(std::vector<SessionID>,
              GetTabsAssociatedWithTask,
              (const base::Uuid&),
              (const, override));
  MOCK_METHOD(void,
              ClearAllTabAssociationsForTask,
              (const base::Uuid&),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetAiThreadControllerDelegate,
              (),
              (override));
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_MOCK_CONTEXTUAL_TASKS_SERVICE_H_
