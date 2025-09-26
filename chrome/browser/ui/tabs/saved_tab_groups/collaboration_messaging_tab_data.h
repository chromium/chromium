// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_TAB_DATA_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_TAB_DATA_H_

#include "base/callback_list.h"
#include "base/check_is_test.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/collaboration/public/messaging/message.h"
#include "ui/base/models/image_model.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class Profile;

namespace views {
class Widget;
}  // namespace views

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace tab_groups {

using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::PersistentMessage;

class CollaborationMessagingTabData {
 public:
  DECLARE_USER_DATA(CollaborationMessagingTabData);

  using CallbackList = base::RepeatingCallbackList<void()>;

  explicit CollaborationMessagingTabData(tabs::TabInterface* tab);

  CollaborationMessagingTabData(CollaborationMessagingTabData& other) = delete;
  CollaborationMessagingTabData& operator=(
      CollaborationMessagingTabData& other) = delete;
  CollaborationMessagingTabData(CollaborationMessagingTabData&& other) = delete;
  CollaborationMessagingTabData& operator=(
      CollaborationMessagingTabData&& other) = delete;
  ~CollaborationMessagingTabData();

  static CollaborationMessagingTabData* From(tabs::TabInterface* tab);

  void SetMessage(PersistentMessage message);
  void ClearMessage(PersistentMessage message);
  bool HasMessage() const {
    return !given_name_.empty() &&
           collaboration_event_ != CollaborationEvent::UNDEFINED;
  }

  // Testing only. Method for setting the avatar image fetch result.
  // Calling SetMessage after this avoids a network request and commits
  // the message data synchronously.
  void set_mocked_avatar_for_testing(std::optional<gfx::Image> mock_avatar) {
    mock_avatar_for_testing_ = mock_avatar;
  }

  gfx::Image* get_avatar_for_testing() {
    CHECK_IS_TEST();
    return &avatar_;
  }

  // Register a callback to be notified when the message changes.
  base::CallbackListSubscription RegisterMessageChangedCallback(
      CallbackList::CallbackType cb);

  std::u16string given_name() {
    CHECK(HasMessage());
    return given_name_;
  }

  // TODO(crbug.com/430025519): Remove this method after migrating to new page
  // actions.
  // Get the image to use when displaying the current message in the
  // page action.
  ui::ImageModel GetPageActionImage(const views::Widget* widget) const;

  // Helper method for getting page action image.
  ui::ImageModel GetPageActionImage(
      float scale_factor,
      const ui::ColorProvider* color_provider) const;

  // Get the image to use when displaying the current message in the
  // hovercard container.
  ui::ImageModel GetHoverCardImage(const views::Widget* widget) const;

  CollaborationEvent collaboration_event() {
    CHECK(HasMessage());
    return collaboration_event_;
  }

  base::WeakPtr<CollaborationMessagingTabData> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(CollaborationMessagingTabDataTest,
                           IgnoresRequestsWhenMessageIsCleared);
  FRIEND_TEST_ALL_PREFIXES(CollaborationMessagingTabDataTest,
                           IgnoresRequestsWhenMessageIsChanged);
  FRIEND_TEST_ALL_PREFIXES(CollaborationMessagingTabDataTest,
                           IgnoresMessageWithoutUser);

  // Creates the image model for the collaboration avatar, falling back to a
  // generic icon if needed.
  ui::ImageModel GetImage(float scale_factor,
                          const ui::ColorProvider* color_provider,
                          int icon_width,
                          bool add_border) const;

  // Notify callback list that a new message has been committed.
  void NotifyMessageChanged();

  // Fetch the avatar image and commit the result, if possible.
  void FetchAvatar(PersistentMessage message);

  // Set the message data to be displayed and notify the callback list.
  void CommitMessage(PersistentMessage message, const gfx::Image& avatar);

  // Create image to use as the fallback when the avatar image is empty.
  ui::ImageModel CreateSizedFallback(float scale_factor,
                                     const ui::ColorProvider* color_provider,
                                     int icon_width,
                                     bool add_border) const;

  raw_ptr<Profile> profile_;

  // The cached message while a request is in flight. This is used to
  // verify that the message has not changed while the image was being
  // requested.
  std::optional<PersistentMessage> message_to_commit_ = std::nullopt;

  // Contains the given name of the user who triggered the event.
  std::u16string given_name_;

  // Contains the type of event this message describes.
  CollaborationEvent collaboration_event_ = CollaborationEvent::UNDEFINED;

  // Contains the avatar returned from the avatar fetching service.
  gfx::Image avatar_;

  // Testing purposes only. Contains a mock image to use in lieu of making
  // a network request for the avatar.
  std::optional<gfx::Image> mock_avatar_for_testing_ = std::nullopt;

  // Listeners to notify when the message for this tab changes.
  CallbackList message_changed_callback_list_;

  ui::ScopedUnownedUserData<CollaborationMessagingTabData>
      scoped_unowned_user_data_;

  // Must be the last member.
  base::WeakPtrFactory<CollaborationMessagingTabData> weak_factory_{this};
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_TAB_DATA_H_
