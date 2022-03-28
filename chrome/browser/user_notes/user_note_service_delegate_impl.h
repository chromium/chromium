// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_NOTES_USER_NOTE_SERVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_USER_NOTES_USER_NOTE_SERVICE_DELEGATE_IMPL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/user_notes/interfaces/user_note_service_delegate.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace user_notes {

class UserNotesUI;

// Implementation of the UserNoteServiceDelegate interface for non-mobile
// Chrome.
class UserNoteServiceDelegateImpl : public UserNoteServiceDelegate {
 public:
  explicit UserNoteServiceDelegateImpl(Profile* profile);
  UserNoteServiceDelegateImpl(const UserNoteServiceDelegateImpl&) = delete;
  UserNoteServiceDelegateImpl& operator=(const UserNoteServiceDelegateImpl&) =
      delete;
  ~UserNoteServiceDelegateImpl() override;

  // UserNoteServiceDelegate implementation.
  std::vector<content::WebContents*> GetAllWebContents() override;

  UserNotesUI* GetUICoordinatorForWebContents(
      const content::WebContents* wc) override;

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace user_notes

#endif  // CHROME_BROWSER_USER_NOTES_USER_NOTE_SERVICE_DELEGATE_IMPL_H_
