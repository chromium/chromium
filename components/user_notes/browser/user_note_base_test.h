// Copyright 2022 The Chromium Authors
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
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"

namespace user_notes {

// Mock implementation of the renderer's AnnotationAgentContainer interface.
// Tests can use this to simulate and make checks on the renderer end that a
// UserNoteManager interacts with.
class MockAnnotationAgentContainer
    : public blink::mojom::AnnotationAgentContainer {
 public:
  MockAnnotationAgentContainer();
  ~MockAnnotationAgentContainer() override;

  // blink::mojom::AnnotationAgentContainer
  MOCK_METHOD4(CreateAgent,
               void(mojo::PendingRemote<blink::mojom::AnnotationAgentHost>,
                    mojo::PendingReceiver<blink::mojom::AnnotationAgent>,
                    blink::mojom::AnnotationType,
                    const std::string& /*serialized_selector*/));

  MOCK_METHOD2(CreateAgentFromSelection,
               void(blink::mojom::AnnotationType,
                    CreateAgentFromSelectionCallback));

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    is_bound_ = true;
    receiver_.Bind(
        mojo::PendingReceiver<blink::mojom::AnnotationAgentContainer>(
            std::move(handle)));
  }

  bool is_bound() const { return is_bound_; }

 private:
  mojo::Receiver<blink::mojom::AnnotationAgentContainer> receiver_{this};
  bool is_bound_ = false;
};

// Similar to above but for the agent interface.
class MockAnnotationAgent : public blink::mojom::AnnotationAgent {
 public:
  MockAnnotationAgent();
  ~MockAnnotationAgent() override;

  // blink::mojom::AnnotationAgent
  MOCK_METHOD0(ScrollIntoView, void());

  mojo::PendingRemote<blink::mojom::AnnotationAgent>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<blink::mojom::AnnotationAgent> receiver_{this};
};

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

  // Called by SetUp. Creates a basic service with a null delegate and storage.
  // Can be overridden to create a service with a delegate and / or storage.
  virtual void CreateService();

  void AddNewNotesToService(size_t count);

  void AddPartialNotesToService(size_t count);

  // Creates and returns a new UserNoteManager for a new WebContents. Callers
  // can optionally pass a MockAnnotationAgentContainer which the new manager's
  // annotation_agent_container_ will bind to.
  UserNoteManager* ConfigureNewManager(
      MockAnnotationAgentContainer* mock_container = nullptr);

  void AddNewInstanceToManager(UserNoteManager* manager,
                               base::UnguessableToken note_id);

  size_t ManagerCountForId(const base::UnguessableToken& note_id);

  bool DoesModelExist(const base::UnguessableToken& note_id);

  bool DoesPartialModelExist(const base::UnguessableToken& note_id);

  bool DoesManagerExistForId(const base::UnguessableToken& note_id,
                             UserNoteManager* manager);

  size_t ModelMapSize();

  size_t CreationMapSize();

  size_t InstanceMapSize(UserNoteManager* manager);

  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<std::unique_ptr<content::WebContents>> web_contents_list_;
  std::unique_ptr<UserNoteService> note_service_;
  std::vector<base::UnguessableToken> note_ids_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_BASE_TEST_H_
