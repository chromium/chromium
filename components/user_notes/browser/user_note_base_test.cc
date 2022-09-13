// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_base_test.h"

#include <memory>
#include <string>
#include <vector>

#include "components/user_notes/model/user_note_model_test_utils.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_notes {

namespace {

const char kBaseUrl[] = "https://www.example.com/";

}  // namespace

MockAnnotationAgentContainer::MockAnnotationAgentContainer() = default;

MockAnnotationAgentContainer::~MockAnnotationAgentContainer() = default;

MockAnnotationAgent::MockAnnotationAgent() = default;

MockAnnotationAgent::~MockAnnotationAgent() = default;

UserNoteBaseTest::UserNoteBaseTest() {
  scoped_feature_list_.InitAndEnableFeature(user_notes::kUserNotes);
}

UserNoteBaseTest::~UserNoteBaseTest() = default;

void UserNoteBaseTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  CreateService();
}

void UserNoteBaseTest::CreateService() {
  note_service_ = std::make_unique<UserNoteService>(/*delegate=*/nullptr,
                                                    /*storage=*/nullptr);
}

void UserNoteBaseTest::TearDown() {
  // Owned web contentses must be destroyed before the test harness.
  web_contents_list_.clear();
  content::RenderViewHostTestHarness::TearDown();
}

void UserNoteBaseTest::AddNewNotesToService(size_t count) {
  for (size_t i = 0; i < count; ++i) {
    size_t last = note_ids_.size();
    note_ids_.push_back(base::UnguessableToken::Create());
    UserNoteService::ModelMapEntry entry(std::make_unique<UserNote>(
        note_ids_[last], GetTestUserNoteMetadata(), GetTestUserNoteBody(),
        GetTestUserNotePageTarget()));
    note_service_->model_map_.emplace(note_ids_[last], std::move(entry));
  }
}

void UserNoteBaseTest::AddPartialNotesToService(size_t count) {
  for (size_t i = 0; i < count; ++i) {
    size_t last = note_ids_.size();
    note_ids_.push_back(base::UnguessableToken::Create());
    UserNoteService::ModelMapEntry entry(std::make_unique<UserNote>(
        note_ids_[last], GetTestUserNoteMetadata(), GetTestUserNoteBody(),
        GetTestUserNotePageTarget()));
    note_service_->creation_map_.emplace(note_ids_[last], std::move(entry));
  }
}

UserNoteManager* UserNoteBaseTest::ConfigureNewManager(
    MockAnnotationAgentContainer* mock_container) {
  // Create a test frame and navigate it to a unique URL.
  std::unique_ptr<content::WebContents> wc = CreateTestWebContents();
  content::RenderFrameHostTester::For(wc->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      wc.get(),
      GURL(kBaseUrl + base::NumberToString(web_contents_list_.size())));

  std::unique_ptr<service_manager::InterfaceProvider::TestApi> test_api;
  if (mock_container) {
    CHECK(!mock_container->is_bound());
    test_api = std::make_unique<service_manager::InterfaceProvider::TestApi>(
        wc->GetPrimaryMainFrame()->GetRemoteInterfaces());
    test_api->SetBinderForName(
        blink::mojom::AnnotationAgentContainer::Name_,
        base::BindRepeating(&MockAnnotationAgentContainer::Bind,
                            base::Unretained(mock_container)));
  }

  // Create and attach a `UserNoteManager` to the primary page.
  content::Page& page = wc->GetPrimaryPage();
  UserNoteManager::CreateForPage(page, note_service_->GetSafeRef());
  UserNoteManager* note_manager = UserNoteManager::GetForPage(page);
  DCHECK(note_manager);
  web_contents_list_.emplace_back(std::move(wc));

  CHECK(!mock_container || mock_container->is_bound());

  return note_manager;
}

void UserNoteBaseTest::AddNewInstanceToManager(UserNoteManager* manager,
                                               base::UnguessableToken note_id) {
  DCHECK(manager);
  const auto& entry_it = note_service_->model_map_.find(note_id);
  ASSERT_FALSE(entry_it == note_service_->model_map_.end());
  manager->AddNoteInstance(
      UserNoteInstance::Create(entry_it->second.model->GetSafeRef(), manager));
}

size_t UserNoteBaseTest::ManagerCountForId(
    const base::UnguessableToken& note_id) {
  const auto& entry_it = note_service_->model_map_.find(note_id);
  if (entry_it == note_service_->model_map_.end()) {
    return -1;
  }
  return entry_it->second.managers.size();
}

bool UserNoteBaseTest::DoesModelExist(const base::UnguessableToken& note_id) {
  const auto& entry_it = note_service_->model_map_.find(note_id);
  return entry_it != note_service_->model_map_.end();
}

bool UserNoteBaseTest::DoesPartialModelExist(
    const base::UnguessableToken& note_id) {
  const auto& entry_it = note_service_->creation_map_.find(note_id);
  return entry_it != note_service_->creation_map_.end();
}

bool UserNoteBaseTest::DoesManagerExistForId(
    const base::UnguessableToken& note_id,
    UserNoteManager* manager) {
  const auto& model_entry_it = note_service_->model_map_.find(note_id);
  if (model_entry_it == note_service_->model_map_.end()) {
    return false;
  }
  const auto& manager_entry_it = model_entry_it->second.managers.find(manager);
  return manager_entry_it != model_entry_it->second.managers.end();
}

size_t UserNoteBaseTest::ModelMapSize() {
  return note_service_->model_map_.size();
}

size_t UserNoteBaseTest::CreationMapSize() {
  return note_service_->creation_map_.size();
}

size_t UserNoteBaseTest::InstanceMapSize(UserNoteManager* manager) {
  DCHECK(manager);
  return manager->instance_map_.size();
}

}  // namespace user_notes
