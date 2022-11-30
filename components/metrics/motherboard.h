// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_MOTHERBOARD_H_
#define COMPONENTS_METRICS_MOTHERBOARD_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {

class Motherboard final {
 public:
  enum class BiosType { kLegacy, kUefi };
  Motherboard();
  Motherboard(Motherboard&&);
  Motherboard(const Motherboard&) = delete;
  ~Motherboard();

  // The fields below provide details about Motherboard and BIOS on the system.
  //
  // A `nullopt_t` means that the property does not exist/could not be read.
  // A valid value could be an UTF-8 string with characters or an empty string.
  //
  // This `absl::optional` can be mapped directly to the optional proto message
  // field, where the message field is added only if there is a valid value.
  const absl::optional<std::string>& manufacturer() const {
    return manufacturer_;
  }
  const absl::optional<std::string>& model() const { return model_; }
  const absl::optional<std::string>& bios_manufacturer() const {
    return bios_manufacturer_;
  }
  const absl::optional<std::string>& bios_version() const {
    return bios_version_;
  }
  absl::optional<BiosType> bios_type() const { return bios_type_; }

 private:
  absl::optional<std::string> manufacturer_;
  absl::optional<std::string> model_;
  absl::optional<std::string> bios_manufacturer_;
  absl::optional<std::string> bios_version_;
  absl::optional<BiosType> bios_type_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_MOTHERBOARD_H_
