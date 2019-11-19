// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_DATA_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_DATA_MODEL_H_

#include <stddef.h>

#include <string>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/form_group.h"

namespace autofill {

struct AutofillMetadata;

// This class is an interface for the primary data models that back Autofill.
// The information in objects of this class is managed by the
// PersonalDataManager.
class AutofillDataModel : public FormGroup {
 public:
  enum ValidityState {
    // The field has not been validated.
    UNVALIDATED = 0,
    // The field is empty.
    EMPTY = 1,
    // The field is valid.
    VALID = 2,
    // The field is invalid.
    INVALID = 3,
    // The validation for the field is unsupported.
    UNSUPPORTED = 4,
  };

  enum ValidationSource {
    // The validity state is according to the client validation.
    CLIENT = 0,
    // The validity state is according to the server validation.
    SERVER = 1,
  };

  AutofillDataModel(const std::string& guid, const std::string& origin);
  ~AutofillDataModel() override;

  // Returns true if the data in this model was entered directly by the user,
  // rather than automatically aggregated.
  bool IsVerified() const;

  std::string guid() const { return guid_; }
  void set_guid(const std::string& guid) { guid_ = guid; }

  std::string origin() const { return origin_; }
  void set_origin(const std::string& origin) { origin_ = origin; }

  size_t use_count() const { return use_count_; }
  void set_use_count(size_t count) { use_count_ = count; }

  // Writing in and reading from database converts dates between time_t and
  // Time, therefore the microseconds get lost. Therefore, we need to round the
  // dates to seconds for both |use_date_| and |modification_date_|.
  const base::Time& use_date() const { return use_date_; }
  void set_use_date(const base::Time& time) { use_date_ = time; }

  bool UseDateEqualsInSeconds(const AutofillDataModel* other) const;

  const base::Time& modification_date() const { return modification_date_; }
  // This should only be called from database code.
  void set_modification_date(const base::Time& time) {
    modification_date_ = time;
  }

  // Compares two data models according to their frecency score. The score uses
  // a combination of frequency and recency to determine the relevance of the
  // profile. |comparison_time_| allows consistent sorting throughout the
  // comparisons.
  bool HasGreaterFrecencyThan(const AutofillDataModel* other,
                              base::Time comparison_time) const;

  // Gets the metadata associated with this autofill data model.
  virtual AutofillMetadata GetMetadata() const;

  // Sets the |use_count_| and |use_date_| of this autofill data model. Returns
  // whether the metadata was set.
  virtual bool SetMetadata(const AutofillMetadata metadata);

  // Returns whether the data model is deletable: if it has not been used for
  // longer than |kDisusedCreditCardDeletionTimeDelta|.
  virtual bool IsDeletable() const;

  // Returns the validity state of the specified autofill type.
  virtual ValidityState GetValidityState(ServerFieldType type,
                                         ValidationSource source) const;

  // Check for the validity of the data. Leave the field empty if the data is
  // invalid and the relevant feature is enabled.
  virtual bool ShouldSkipFillingOrSuggesting(ServerFieldType type) const;

 protected:
  // Called to update |use_count_| and |use_date_| when this data model is
  // the subject of user interaction (usually, when it's used to fill a form).
  void RecordUse();

  // Returns a score based on both the recency (relative to |time|) and
  // frequency for the model. The score is a negative number where a higher
  // value is more relevant. |time| is passed as a parameter to ensure
  // consistent results.
  double GetFrecencyScore(base::Time time) const;

 private:
  // A globally unique ID for this object.
  std::string guid_;

  // The origin of this data.  This should be
  //   (a) a web URL for the domain of the form from which the data was
  //       automatically aggregated, e.g. https://www.example.com/register,
  //   (b) some other non-empty string, which cannot be interpreted as a web
  //       URL, identifying the origin for non-aggregated data, or
  //   (c) an empty string, indicating that the origin for this data is unknown.
  std::string origin_;

  // The number of times this model has been used.
  size_t use_count_;

  // The last time the model was used, rounded in seconds. Any change should
  // use set_previous_use_date()
  base::Time use_date_;

  // The last time data in the model was modified, rounded in seconds. Any
  // change should use set_previous_modification_date()
  base::Time modification_date_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_DATA_MODEL_H_
