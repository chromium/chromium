// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/prefs/pref_member.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

class AutofillDriver;
class AutofillExternalDelegate;
class AutofillClient;
struct FormData;

// Per-tab Autocomplete history manager. Handles receiving form data
// from the renderer and the storing and retrieving of form data
// through WebDataServiceBase.
class AutocompleteHistoryManager : public WebDataServiceConsumer {
 public:
  AutocompleteHistoryManager(AutofillDriver* driver,
                             AutofillClient* autofill_client);
  ~AutocompleteHistoryManager() override;

  // Pass-through functions that are called by AutofillManager, after it has
  // dispatched a message.
  virtual void OnGetAutocompleteSuggestions(
      int query_id,
      const base::string16& name,
      const base::string16& prefix,
      const std::string& form_control_type);
  virtual void OnWillSubmitForm(const FormData& form);

  // Cancels the currently pending WebDataService query, if there is one.
  void CancelPendingQuery();

  // Must be public for the external delegate to use.
  void OnRemoveAutocompleteEntry(const base::string16& name,
                                 const base::string16& value);

  // Sets our external delegate.
  void SetExternalDelegate(AutofillExternalDelegate* delegate);

 protected:
  friend class AutofillManagerTest;

  // Sends the stored suggestions plus the new autocomplete results for display
  // in the Autofill popup. The parameter may be null if there are no new
  // autocomplete additions.
  void SendSuggestions(const std::vector<base::string16>* new_results);

 private:
  FRIEND_TEST_ALL_PREFIXES(AutocompleteHistoryManagerTest,
                           AutocompleteUMAQueryCreated);

  // The class measure the percentage field that triggers the query and the
  // percentage field that has the suggestion.
  class UMARecorder {
   public:
    UMARecorder() = default;
    ~UMARecorder() = default;

    void OnGetAutocompleteSuggestions(
        const base::string16& name,
        WebDataServiceBase::Handle pending_query_handle);
    void OnWebDataServiceRequestDone(
        WebDataServiceBase::Handle pending_query_handle,
        bool has_suggestion);

   private:
    // The query handle should be measured for UMA.
    WebDataServiceBase::Handle measuring_query_handle_ = 0;

    // The name of field that is currently measured, we don't repeatedly measure
    // the query of the same field while user is filling the field.
    base::string16 measuring_name_;

    DISALLOW_COPY_AND_ASSIGN(UMARecorder);
  };

  // WebDataServiceConsumer implementation.
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // Provides driver-level context. Must outlive this object.
  AutofillDriver* driver_;
  scoped_refptr<AutofillWebDataService> database_;

  // When the manager makes a request from WebDataServiceBase, the database is
  // queried on another thread, we record the query handle until we get called
  // back.
  WebDataServiceBase::Handle pending_query_handle_;
  int query_id_;

  // Delegate to perform external processing (display, selection) on
  // our behalf.  Weak.
  AutofillExternalDelegate* external_delegate_;

  // Delegate to provide whether or not autocomplete functionality is enabled.
  AutofillClient* const autofill_client_;

  // Whether IPC is sent.
  bool send_ipc_;

  UMARecorder uma_recorder_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteHistoryManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
