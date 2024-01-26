// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_USB_PRINTER_ID_H_
#define CHROMEOS_PRINTING_USB_PRINTER_ID_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"

namespace chromeos {

// This class parses and holds the IEEE 1284 Device ID string as queried
// from a USB-connected printer.
class COMPONENT_EXPORT(CHROMEOS_PRINTING) UsbPrinterId {
 public:
  UsbPrinterId();
  UsbPrinterId(const UsbPrinterId& other);
  ~UsbPrinterId();

  // Expects |printer_id_data| to contain the data portion response to a USB
  // Printer Class-Specific GET_DEVICE_ID Request.
  explicit UsbPrinterId(base::span<const uint8_t> printer_id_data);

  // Accessors.
  const std::string& make() const { return make_; }
  const std::string& model() const { return model_; }
  const std::vector<std::string>& command_set() const { return command_set_; }
  const std::string& raw_id() const { return raw_id_; }

  // Setters (only used in testing).
  void set_make(std::string make) { make_ = make; }
  void set_model(std::string model) { model_ = model; }
  void set_command_set(std::vector<std::string> command_set) {
    command_set_ = std::move(command_set);
  }

 private:
  std::string raw_id_;
  std::string make_;
  std::string model_;

  // List of supported document formats (MIME types).
  std::vector<std::string> command_set_;

  // Holds the fully parsed IEEE 1284 Device ID.
  std::map<std::string, std::vector<std::string>> id_mappings_;
};

// Expects data to hold a IEEE 1284 Device ID. Parses |data| and returns the
// resulting key-value(s) pairs.
COMPONENT_EXPORT(CHROMEOS_PRINTING)
std::map<std::string, std::vector<std::string>> BuildDeviceIdMapping(
    base::span<const uint8_t> data);

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_USB_PRINTER_ID_H_
