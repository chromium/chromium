// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_EDIT_SEARCH_ENGINE_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_EDIT_SEARCH_ENGINE_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/native_widget_types.h"

class Profile;
class TemplateURL;

class EditSearchEngineControllerDelegate {
 public:
  // Invoked from the EditSearchEngineController when the user accepts the
  // edits. NOTE: |template_url| is the value supplied to
  // EditSearchEngineController's constructor, and may be nullptr. A nullptr
  // value indicates a new TemplateURL should be created rather than modifying
  // an existing TemplateURL.
  virtual void OnEditedKeyword(TemplateURL* template_url,
                               const std::u16string& title,
                               const std::u16string& keyword,
                               const std::string& url) = 0;

 protected:
  virtual ~EditSearchEngineControllerDelegate() {}
};

// EditSearchEngineController provides the core platform independent logic
// for the Edit Search Engine dialog.
class EditSearchEngineController {
 public:
  // The |template_url| and/or |edit_keyword_delegate| may be nullptr.
  EditSearchEngineController(
      TemplateURL* template_url,
      EditSearchEngineControllerDelegate* edit_keyword_delegate,
      Profile* profile);

  EditSearchEngineController(const EditSearchEngineController&) = delete;
  EditSearchEngineController& operator=(const EditSearchEngineController&) =
      delete;

  ~EditSearchEngineController() {}

  // Returns true if the value of |title_input| is a valid search engine name.
  bool IsTitleValid(const std::u16string& title_input) const;

  // Returns true if the value of |url_input| represents a valid search engine
  // URL. The URL is valid if it contains no search terms and is a valid
  // url, or if it contains a search term and replacing that search term with a
  // character results in a valid url.
  bool IsURLValid(const std::string& url_input) const;

  // Returns true if the value of |keyword_input| represents a valid keyword.
  // The keyword is valid if it is non-empty and does not conflict with an
  // existing entry. NOTE: this is just the keyword, not the title and url.
  bool IsKeywordValid(const std::u16string& keyword_input) const;

  // Completes the add or edit of a search engine.
  void AcceptAddOrEdit(const std::u16string& title_input,
                       const std::u16string& keyword_input,
                       const std::string& url_input);

  // Deletes an unused TemplateURL, if its add was cancelled and it's not
  // already owned by the TemplateURLService.
  void CleanUpCancelledAdd();

  // Accessors.
  const TemplateURL* template_url() const { return template_url_; }
  Profile* profile() const { return profile_; }

 private:
  // Fixes up and returns the URL the user has input. The returned URL is
  // suitable for use by TemplateURL.
  std::string GetFixedUpURL(const std::string& url_input) const;

  // The TemplateURL we're displaying information for. It may be nullptr. If we
  // have a keyword_editor_view, we assume that this TemplateURL is already in
  // the TemplateURLService; if not, we assume it isn't.
  raw_ptr<TemplateURL> template_url_;

  // We may have been created by this, in which case we will call back to it on
  // success to add/modify the entry.  May be nullptr.
  raw_ptr<EditSearchEngineControllerDelegate> edit_keyword_delegate_;

  // Profile whose TemplateURLService we're modifying.
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_EDIT_SEARCH_ENGINE_CONTROLLER_H_
