// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_EXPERIMENT_LABELS_H_
#define CHROME_INSTALLER_UTIL_EXPERIMENT_LABELS_H_

#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/time/time.h"

namespace installer {

// A wrapper around an Omaha "experiment_labels" value. No validation is
// performed on any values. For reference, see
// https://github.com/google/omaha/blob/master/omaha/common/experiment_labels.cc#L16.
class ExperimentLabels {
 public:
  explicit ExperimentLabels(const std::wstring& value);

  ExperimentLabels(const ExperimentLabels&) = delete;
  ExperimentLabels& operator=(const ExperimentLabels&) = delete;

  // Returns the experiment_labels string containing the individual labels.
  const std::wstring& value() const { return value_; }

  // Returns a StringPiece pointing to the value for a given label, or an empty
  // StringPiece if it is not present in the instance's value. Note: the
  // ExperimentLabels instance must outlive the piece returned, and the piece is
  // invalidated by any call to SetValueForLabel.
  base::WStringPiece GetValueForLabel(base::WStringPiece label_name) const;

  // Sets the value of a given label, overwriting a previous value if found.
  // The label's expiration date is set to the current time plus |lifetime|.
  void SetValueForLabel(base::WStringPiece label_name,
                        base::WStringPiece label_value,
                        base::TimeDelta lifetime);

  // Sets the value of a given label, overwriting a previous value if found.
  void SetValueForLabel(base::WStringPiece label_name,
                        base::WStringPiece label_value,
                        base::Time expiration);

 private:
  // A label's full contents ("name=value|expiration") and value within
  // |value_|.
  using LabelAndValue = std::pair<base::WStringPiece, base::WStringPiece>;

  // Returns the label and value named |label_name|, or empty string pieces if
  // not found.
  LabelAndValue FindLabel(base::WStringPiece label_name) const;

  // The raw experiment_labels string.
  std::wstring value_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_EXPERIMENT_LABELS_H_
