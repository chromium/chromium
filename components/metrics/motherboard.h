// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_MOTHERBOARD_H_
#define COMPONENTS_METRICS_MOTHERBOARD_H_

#include <optional>
#include <string>

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
  // This `std::optional` can be mapped directly to the optional proto message
  // field, where the message field is added only if there is a valid value.
  const std::optional<std::string>& manufacturer() const {
    return manufacturer_;
  }
  const std::optional<std::string>& model() const { return model_; }
  const std::optional<std::string>& bios_manufacturer() const {
    return bios_manufacturer_;
  }
  const std::optional<std::string>& bios_version() const {
    return bios_version_;
  }
  std::optional<BiosType> bios_type() const { return bios_type_; }

 private:
  std::optional<std::string> manufacturer_;
  std::optional<std::string> model_;
  std::optional<std::string> bios_manufacturer_;
  std::optional<std::string> bios_version_;
  std::optional<BiosType> bios_type_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_MOTHERBOARD_H_
