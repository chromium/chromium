// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_BASE_TEST_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_BASE_TEST_H_

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/browser/user_note_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"

namespace user_notes {

// A base test harness for User Notes unit tests. The harness sets up a note
// service and exposes methods to create new note models, as well as methods to
// create and manipulate note managers attached to mock pages.
class UserNoteBaseTest : public content::RenderViewHostTestHarness {
 public:
  UserNoteBaseTest();
  ~UserNoteBaseTest() override;

 protected:
  void SetUp() override;

  void TearDown() override;

  void AddNewNotesToService(size_t count);

  UserNoteManager* ConfigureNewManager();

  void AddNewInstanceToManager(UserNoteManager* manager,
                               base::UnguessableToken note_id);

  size_t ManagerCountForId(const base::UnguessableToken& note_id);

  bool DoesModelExist(const base::UnguessableToken& note_id);

  bool DoesManagerExistForId(const base::UnguessableToken& note_id,
                             UserNoteManager* manager);

  size_t ModelMapSize();

  size_t InstanceMapSize(UserNoteManager* manager);

  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<std::unique_ptr<content::WebContents>> web_contents_list_;
  std::unique_ptr<UserNoteService> note_service_;
  std::vector<base::UnguessableToken> note_ids_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_BASE_TEST_H_
