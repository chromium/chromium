// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/print_dialog_linux_portal.h"

#include <dbus/dbus-shared.h>
#include <dbus/dbus.h>
#include <fcntl.h>
#include <sys/types.h>

#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/write_value.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/request.h"
#include "components/strings/grit/components_strings.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "printing/metafile.h"
#include "printing/print_settings.h"
#include "printing/units.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_delegate.h"

namespace printing {

namespace {

constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/desktop";
constexpr char kPrintInterfaceName[] = "org.freedesktop.portal.Print";

constexpr char kMethodPreparePrint[] = "PreparePrint";
constexpr char kMethodPrint[] = "Print";

constexpr char kDBusInterfaceName[] = "org.freedesktop.DBus";
constexpr char kMethodGetId[] = "GetId";

// Keys for settings dictionary. Defined in org.freedesktop.portal.Print spec.
// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Print.html
constexpr char kKeyCollate[] = "collate";
constexpr char kKeyCopies[] = "n-copies";
constexpr char kKeyDefaultSource[] = "default-source";
constexpr char kKeyDither[] = "dither";
constexpr char kKeyDuplex[] = "duplex";
constexpr char kKeyFinishings[] = "finishings";
constexpr char kKeyMediaType[] = "media-type";
constexpr char kKeyNumberUpLayout[] = "number-up-layout";
constexpr char kKeyNumberUp[] = "number-up";
constexpr char kKeyOrientation[] = "orientation";
constexpr char kKeyOutputBasename[] = "output-basename";
constexpr char kKeyOutputBin[] = "output-bin";
constexpr char kKeyOutputFileFormat[] = "output-file-format";
constexpr char kKeyOutputUri[] = "output-uri";
constexpr char kKeyPageRanges[] = "page-ranges";
constexpr char kKeyPageSet[] = "page-set";
constexpr char kKeyPaperFormat[] = "paper-format";
constexpr char kKeyPaperHeight[] = "paper-height";
constexpr char kKeyPaperWidth[] = "paper-width";
constexpr char kKeyPrintPages[] = "print-pages";
constexpr char kKeyPrinterLpi[] = "printer-lpi";
constexpr char kKeyQuality[] = "quality";
constexpr char kKeyResolutionX[] = "resolution-x";
constexpr char kKeyResolutionY[] = "resolution-y";
constexpr char kKeyResolution[] = "resolution";
constexpr char kKeyReverse[] = "reverse";
constexpr char kKeyScale[] = "scale";
constexpr char kKeyUseColor[] = "use-color";

constexpr char kKeyName[] = "Name";
constexpr char kKeyPpdName[] = "PPDName";

constexpr char kValueAll[] = "all";
constexpr char kValueFalse[] = "false";
constexpr char kValueHorizontal[] = "horizontal";
constexpr char kValueLandscape[] = "landscape";
constexpr char kValuePortrait[] = "portrait";
constexpr char kValueRanges[] = "ranges";
constexpr char kValueReverseLandscape[] = "reverse_landscape";
constexpr char kValueSelection[] = "selection";
constexpr char kValueSimplex[] = "simplex";
constexpr char kValueTrue[] = "true";
constexpr char kValueVertical[] = "vertical";

constexpr char kPageRangeSeparator[] = ",";
constexpr char kRangeSeparator[] = "-";
constexpr char kResolutionDimensionSeparator[] = "x";

// Standard scale factor for the print preview.
constexpr double kScaleFactor = 100.0;

// Keys for page setup dictionary
constexpr char kKeyPageSetupHeight[] = "Height";
constexpr char kKeyPageSetupMarginBottom[] = "MarginBottom";
constexpr char kKeyPageSetupMarginLeft[] = "MarginLeft";
constexpr char kKeyPageSetupMarginRight[] = "MarginRight";
constexpr char kKeyPageSetupMarginTop[] = "MarginTop";
constexpr char kKeyPageSetupOrientation[] = "Orientation";
constexpr char kKeyPageSetupWidth[] = "Width";

// Keys for options/results
constexpr char kKeyModal[] = "modal";
constexpr char kKeyPageSetup[] = "page-setup";
constexpr char kKeySettings[] = "settings";
constexpr char kKeyToken[] = "token";

constexpr int kXdgPortalRequiredVersion = 3;

constexpr char16_t kPrintDialogLinuxPortalDeviceName[] =
    u"PrintDialogLinuxPortal";

void WriteDataToPipe(base::ScopedFD fd, std::vector<char> data) {
  base::File file(std::move(fd));
  if (!file.WriteAtCurrentPosAndCheck(base::as_byte_span(data))) {
    LOG(ERROR) << "Failed to save metafile to pipe";
  }
}

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
// Serializes a D-Bus value into a byte vector.
template <typename T>
std::vector<uint8_t> SerializeValue(const T& value) {
  std::unique_ptr<dbus::Response> message = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(message.get());
  dbus_utils::WriteValue(writer, value);

  char* marshalled_data = nullptr;
  int len = 0;
  if (!dbus_message_marshal(message->raw_message(), &marshalled_data, &len)) {
    return {};
  }

  // SAFETY: dbus_message_marshal guarantees that `marshalled_data` points to a
  // buffer of `len` bytes.
  auto data_span =
      UNSAFE_BUFFERS(base::span(marshalled_data, static_cast<size_t>(len)));
  auto bytes_span = base::as_bytes(data_span);
  std::vector<uint8_t> result(bytes_span.begin(), bytes_span.end());

  dbus_free(marshalled_data);
  return result;
}

// Deserializes a D-Bus value from a byte span.
template <typename T>
std::optional<T> DeserializeValue(base::span<const uint8_t> data) {
  DBusError error;
  dbus_error_init(&error);
  DBusMessage* raw_message = dbus_message_demarshal(
      reinterpret_cast<const char*>(data.data()), data.size(), &error);
  if (dbus_error_is_set(&error)) {
    dbus_error_free(&error);
    return std::nullopt;
  }
  if (!raw_message) {
    return std::nullopt;
  }

  std::unique_ptr<dbus::Response> message =
      dbus::Response::FromRawMessage(raw_message);
  dbus::MessageReader reader(message.get());
  return dbus_utils::ReadValue<T>(reader);
}
#endif

dbus_xdg::Dictionary ConvertSettingsToDictionary(
    const PrintSettings& settings) {
  dbus_xdg::Dictionary dict;

  // Orientation
  dict[kKeyOrientation] = dbus_utils::Variant::Wrap<"s">(
      settings.landscape() ? kValueLandscape : kValuePortrait);

  // Copies
  dict[kKeyCopies] =
      dbus_utils::Variant::Wrap<"s">(base::NumberToString(settings.copies()));

  // Collate
  dict[kKeyCollate] = dbus_utils::Variant::Wrap<"s">(
      settings.collate() ? kValueTrue : kValueFalse);

  // Color
  dict[kKeyUseColor] = dbus_utils::Variant::Wrap<"s">(
      settings.color() != mojom::ColorModel::kGray ? kValueTrue : kValueFalse);

  // Duplex
  std::string duplex;
  switch (settings.duplex_mode()) {
    case mojom::DuplexMode::kSimplex:
      duplex = kValueSimplex;
      break;
    case mojom::DuplexMode::kLongEdge:
      duplex = kValueVertical;
      break;
    case mojom::DuplexMode::kShortEdge:
      duplex = kValueHorizontal;
      break;
    default:
      duplex = kValueSimplex;
  }
  dict[kKeyDuplex] = dbus_utils::Variant::Wrap<"s">(duplex);

  // Resolution
  if (settings.dpi_horizontal() > 0 && settings.dpi_vertical() > 0) {
    std::string res = base::NumberToString(settings.dpi_horizontal());
    if (settings.dpi_horizontal() != settings.dpi_vertical()) {
      res += kResolutionDimensionSeparator +
             base::NumberToString(settings.dpi_vertical());
    }
    dict[kKeyResolution] = dbus_utils::Variant::Wrap<"s">(res);
  }

  // Ranges
  if (settings.ranges().empty()) {
    dict[kKeyPrintPages] = dbus_utils::Variant::Wrap<"s">(
        settings.selection_only() ? kValueSelection : kValueAll);
  } else {
    dict[kKeyPrintPages] = dbus_utils::Variant::Wrap<"s">(kValueRanges);
    std::vector<std::string> ranges_strs;
    for (const auto& range : settings.ranges()) {
      if (range.from == range.to) {
        ranges_strs.push_back(base::NumberToString(range.from));
      } else {
        ranges_strs.push_back(base::NumberToString(range.from) +
                              kRangeSeparator + base::NumberToString(range.to));
      }
    }
    dict[kKeyPageRanges] = dbus_utils::Variant::Wrap<"s">(
        base::JoinString(ranges_strs, kPageRangeSeparator));
  }

  return dict;
}

dbus_xdg::Dictionary ConvertPageSetupToDictionary(
    const PrintSettings& settings) {
  dbus_xdg::Dictionary dict;

  const auto& media = settings.requested_media();
  if (!media.size_microns.IsEmpty()) {
    dict[kKeyPageSetupWidth] = dbus_utils::Variant::Wrap<"d">(
        static_cast<double>(media.size_microns.width()) / kMicronsPerMm);
    dict[kKeyPageSetupHeight] = dbus_utils::Variant::Wrap<"d">(
        static_cast<double>(media.size_microns.height()) / kMicronsPerMm);
  }

  if (!media.vendor_id.empty()) {
    dict[kKeyPpdName] = dbus_utils::Variant::Wrap<"s">(media.vendor_id);
    dict[kKeyName] = dbus_utils::Variant::Wrap<"s">(media.vendor_id);
  }

  // Margins
  double top_mm = 0;
  double bottom_mm = 0;
  double left_mm = 0;
  double right_mm = 0;
  bool margins_set = false;

  if (settings.margin_type() == mojom::MarginType::kCustomMargins) {
    const auto& margins = settings.requested_custom_margins_in_microns();
    top_mm = static_cast<double>(margins.top) / kMicronsPerMm;
    bottom_mm = static_cast<double>(margins.bottom) / kMicronsPerMm;
    left_mm = static_cast<double>(margins.left) / kMicronsPerMm;
    right_mm = static_cast<double>(margins.right) / kMicronsPerMm;
    margins_set = true;
  } else {
    // Attempt to calculate from device units if DPI is available.
    int dpi = settings.dpi();
    if (dpi > 0) {
      const auto& page_setup = settings.page_setup_device_units();
      const auto& physical_size = page_setup.physical_size();
      const auto& printable_area = page_setup.printable_area();

      if (!physical_size.IsEmpty()) {
        double conversion = kMmPerInch / dpi;

        left_mm = printable_area.x() * conversion;
        top_mm = printable_area.y() * conversion;
        right_mm =
            (physical_size.width() - printable_area.right()) * conversion;
        bottom_mm =
            (physical_size.height() - printable_area.bottom()) * conversion;
        margins_set = true;
      }
    }
  }

  if (margins_set) {
    dict[kKeyPageSetupMarginTop] = dbus_utils::Variant::Wrap<"d">(top_mm);
    dict[kKeyPageSetupMarginBottom] = dbus_utils::Variant::Wrap<"d">(bottom_mm);
    dict[kKeyPageSetupMarginLeft] = dbus_utils::Variant::Wrap<"d">(left_mm);
    dict[kKeyPageSetupMarginRight] = dbus_utils::Variant::Wrap<"d">(right_mm);
  }

  // Orientation matches settings
  dict[kKeyPageSetupOrientation] = dbus_utils::Variant::Wrap<"s">(
      settings.landscape() ? kValueLandscape : kValuePortrait);
  return dict;
}

void ParseSettingsFromDictionary(dbus_xdg::Dictionary& dict,
                                 PrintSettings* settings) {
  if (auto it = dict.find(kKeyOrientation); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      settings->SetOrientation(*val == kValueLandscape ||
                               *val == kValueReverseLandscape);
    }
  }

  if (auto it = dict.find(kKeyCopies); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      int copies = 0;
      if (base::StringToInt(*val, &copies)) {
        settings->set_copies(copies);
      }
    }
  }

  if (auto it = dict.find(kKeyCollate); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      settings->set_collate(*val == kValueTrue);
    }
  }

  if (auto it = dict.find(kKeyUseColor); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      settings->set_color(*val == kValueTrue ? mojom::ColorModel::kColor
                                             : mojom::ColorModel::kGray);
    }
  }

  if (auto it = dict.find(kKeyDuplex); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      if (*val == kValueSimplex) {
        settings->set_duplex_mode(mojom::DuplexMode::kSimplex);
      } else if (*val == kValueVertical) {
        settings->set_duplex_mode(mojom::DuplexMode::kLongEdge);
      } else if (*val == kValueHorizontal) {
        settings->set_duplex_mode(mojom::DuplexMode::kShortEdge);
      }
    }
  }

  if (auto it = dict.find(kKeyResolution); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      int dpi_x = 0;
      int dpi_y = 0;
      std::string_view s = *val;
      auto pos = s.find(kResolutionDimensionSeparator);
      if (pos == std::string_view::npos) {
        if (base::StringToInt(s, &dpi_x) && dpi_x > 0) {
          settings->set_dpi(dpi_x);
        }
      } else {
        std::string_view sx = s.substr(0, pos);
        std::string_view sy = s.substr(pos + 1);
        if (base::StringToInt(sx, &dpi_x) && base::StringToInt(sy, &dpi_y) &&
            dpi_x > 0 && dpi_y > 0) {
          settings->set_dpi_xy(dpi_x, dpi_y);
        }
      }
    }
  }

  if (auto it = dict.find(kKeyScale); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      double scale = 0;
      if (base::StringToDouble(*val, &scale)) {
        settings->set_scale_factor(scale / kScaleFactor);
      }
    }
  }

  if (auto it = dict.find(kKeyPrintPages); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      if (*val == kValueSelection) {
        settings->set_selection_only(true);
      } else if (*val == kValueAll) {
        settings->set_selection_only(false);
        settings->set_ranges({});
      }
    }
  }

  if (auto it = dict.find(kKeyPageRanges); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      PageRanges ranges;
      for (const auto& range_str :
           base::SplitString(*val, kPageRangeSeparator, base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_ALL)) {
        std::vector<std::string> parts =
            base::SplitString(range_str, kRangeSeparator, base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL);
        int from = 0;
        int to = 0;
        if (parts.size() == 1 && base::StringToInt(parts[0], &from)) {
          if (from >= 0) {
            ranges.push_back(
                {static_cast<uint32_t>(from), static_cast<uint32_t>(from)});
          }
        } else if (parts.size() == 2 && base::StringToInt(parts[0], &from) &&
                   base::StringToInt(parts[1], &to)) {
          if (from >= 0 && to >= 0) {
            ranges.push_back(
                {static_cast<uint32_t>(from), static_cast<uint32_t>(to)});
          }
        }
      }
      settings->set_ranges(ranges);
    }
  }

  if (auto it = dict.find(kKeyMediaType); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      settings->set_media_type(*val);
    }
  }

  if (auto it = dict.find(kKeyNumberUp); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      int nup = 1;
      if (base::StringToInt(*val, &nup) && nup > 0) {
        settings->set_pages_per_sheet(nup);
      }
    }
  }

  // Advanced settings mapping
  static constexpr const char* kAdvancedKeys[] = {
      kKeyReverse,        kKeyDither,           kKeyFinishings,
      kKeyPageSet,        kKeyOutputBin,        kKeyOutputUri,
      kKeyOutputBasename, kKeyOutputFileFormat, kKeyPrinterLpi,
      kKeyNumberUpLayout, kKeyPaperFormat,      kKeyPaperWidth,
      kKeyPaperHeight,    kKeyDefaultSource,    kKeyQuality,
      kKeyResolutionX,    kKeyResolutionY,
  };

  for (const char* key : kAdvancedKeys) {
    if (auto it = dict.find(key); it != dict.end()) {
      if (auto val = std::move(it->second).Take<std::string>()) {
        settings->advanced_settings().emplace(key,
                                              base::Value(std::move(*val)));
      }
    }
  }
}

void ParsePageSetupFromDictionary(dbus_xdg::Dictionary& dict,
                                  PrintSettings* settings) {
  // Page setup properties are usually in mm.
  double width_mm = 0;
  double height_mm = 0;

  if (auto it = dict.find(kKeyPageSetupWidth); it != dict.end()) {
    if (auto val = std::move(it->second).Take<double>()) {
      width_mm = *val;
    }
  }
  if (auto it = dict.find(kKeyPageSetupHeight); it != dict.end()) {
    if (auto val = std::move(it->second).Take<double>()) {
      height_mm = *val;
    }
  }

  if (width_mm > 0 && height_mm > 0) {
    PrintSettings::RequestedMedia media = settings->requested_media();
    media.size_microns = gfx::Size(static_cast<int>(width_mm * kMicronsPerMm),
                                   static_cast<int>(height_mm * kMicronsPerMm));
    settings->set_requested_media(media);
  }

  // Margins
  double top_mm = 0;
  double bottom_mm = 0;
  double left_mm = 0;
  double right_mm = 0;
  bool margins_set = false;
  if (auto it = dict.find(kKeyPageSetupMarginTop); it != dict.end()) {
    if (auto val = std::move(it->second).Take<double>()) {
      top_mm = *val;
      margins_set = true;
    }
  }
  if (auto it = dict.find(kKeyPageSetupMarginBottom); it != dict.end()) {
    if (auto val = std::move(it->second).Take<double>()) {
      bottom_mm = *val;
      margins_set = true;
    }
  }
  if (auto it = dict.find(kKeyPageSetupMarginLeft); it != dict.end()) {
    if (auto val = std::move(it->second).Take<double>()) {
      left_mm = *val;
      margins_set = true;
    }
  }
  if (auto it = dict.find(kKeyPageSetupMarginRight); it != dict.end()) {
    if (auto val = std::move(it->second).Take<double>()) {
      right_mm = *val;
      margins_set = true;
    }
  }

  if (margins_set) {
    PageMargins margins;
    margins.top = static_cast<int>(top_mm * kMicronsPerMm);
    margins.bottom = static_cast<int>(bottom_mm * kMicronsPerMm);
    margins.left = static_cast<int>(left_mm * kMicronsPerMm);
    margins.right = static_cast<int>(right_mm * kMicronsPerMm);
    settings->SetCustomMargins(margins);
  }

  if (auto it = dict.find(kKeyPageSetupOrientation); it != dict.end()) {
    if (auto val = std::move(it->second).Take<std::string>()) {
      settings->SetOrientation(*val == kValueLandscape ||
                               *val == kValueReverseLandscape);
    }
  }

  // Calculate and set printer printable area.
  int dpi = settings->dpi();
  if (dpi <= 0) {
    // This should generally be set by ParseSettingsFromDictionary or default.
    dpi = kDefaultPdfDpi;
    settings->set_dpi(dpi);
  }

  // Convert mm to inches (25.4 mm per inch) then to device units (dots).
  gfx::Size physical_size_device_units(
      static_cast<int>(width_mm * dpi / kMmPerInch),
      static_cast<int>(height_mm * dpi / kMmPerInch));

  gfx::Rect printable_area_device_units(
      static_cast<int>(left_mm * dpi / kMmPerInch),
      static_cast<int>(top_mm * dpi / kMmPerInch),
      static_cast<int>((width_mm - left_mm - right_mm) * dpi / kMmPerInch),
      static_cast<int>((height_mm - top_mm - bottom_mm) * dpi / kMmPerInch));

  settings->SetPrinterPrintableArea(physical_size_device_units,
                                    printable_area_device_units,
                                    /*landscape_needs_flip=*/true);
}

void OnPrintResponse(dbus_xdg::Results results) {
  if (!results.has_value()) {
    LOG(ERROR) << "Portal Print failed";
  }
}

void StartPrintRequest(const std::string& title,
                       base::ScopedFD fd,
                       scoped_refptr<dbus::Bus> bus,
                       const std::string& parent_handle,
                       std::optional<uint32_t> token) {
  CHECK(!bus->GetConnectionName().empty());
  dbus::ObjectProxy* portal_proxy = bus->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  dbus_xdg::Dictionary options;
  if (token) {
    options[kKeyToken] = dbus_utils::Variant::Wrap<"u">(*token);
  }

  // Print(parent_window, title, fd, options)
  auto request = std::make_unique<dbus_xdg::Request>(
      bus, portal_proxy, kPrintInterfaceName, kMethodPrint, std::move(options),
      base::BindOnce(&OnPrintResponse), parent_handle, title, std::move(fd));
  // In the OOP print service, the dialog may be destroyed before the print
  // request completes. Release the request to avoid cancelling it.
  request->Release();
}

void OnBusNameAcquired(const std::string& title,
                       base::ScopedFD fd,
                       scoped_refptr<dbus::Bus> bus,
                       const std::string& parent_handle,
                       std::optional<uint32_t> token,
                       dbus_utils::CallMethodResultSig<"s"> results) {
  if (!bus->IsConnected()) {
    // Failed to connect to D-Bus.
    return;
  }

  StartPrintRequest(title, std::move(fd), std::move(bus), parent_handle, token);
}

void EnsureDBusInitialized(const std::string& title,
                           base::ScopedFD fd,
                           scoped_refptr<dbus::Bus> bus,
                           const std::string& parent_handle,
                           std::optional<uint32_t> token) {
  if (bus->IsConnected()) {
    StartPrintRequest(title, std::move(fd), std::move(bus), parent_handle,
                      token);
    return;
  }

  // In the OOP print service, the D-Bus connection may not be initialized.
  // Make a no-op call to initialize it and ensure the bus has a name, which
  // is required for XDG portal requests.
  auto* object_proxy =
      bus->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  dbus_utils::CallMethod<"", "s">(
      object_proxy, kDBusInterfaceName, kMethodGetId,
      base::BindOnce(&OnBusNameAcquired, title, std::move(fd), std::move(bus),
                     parent_handle, token));
}

}  // namespace

PrintDialogLinuxPortal::PrintDialogLinuxPortal(PrintingContextLinux* context,
                                               scoped_refptr<dbus::Bus> bus)
    : context_(context),
      bus_(bus ? std::move(bus) : dbus_thread_linux::GetSharedSessionBus()),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

PrintDialogLinuxPortal::~PrintDialogLinuxPortal() = default;

void PrintDialogLinuxPortal::UseDefaultSettings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (fallback_dialog_) {
    fallback_dialog_->UseDefaultSettings();
    return;
  }

  auto settings = std::make_unique<PrintSettings>();
  settings->set_dpi(kDefaultPdfDpi);
  gfx::Size physical_size(static_cast<int>(kA4WidthInch * kDefaultPdfDpi),
                          static_cast<int>(kA4HeightInch * kDefaultPdfDpi));
  gfx::Rect printable_area(physical_size);
  settings->SetPrinterPrintableArea(physical_size, printable_area, true);
  context_->InitWithSettings(std::move(settings));
}

void PrintDialogLinuxPortal::UpdateSettings(
    std::unique_ptr<PrintSettings> settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (fallback_dialog_) {
    fallback_dialog_->UpdateSettings(std::move(settings));
    return;
  }
  settings_ = std::make_unique<PrintSettings>(*settings);
  context_->InitWithSettings(std::move(settings));
}

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
void PrintDialogLinuxPortal::LoadPrintSettings(const PrintSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (fallback_dialog_) {
    fallback_dialog_->LoadPrintSettings(settings);
    return;
  }
  settings_ = std::make_unique<PrintSettings>(settings);

  const std::vector<uint8_t>* settings_data =
      settings.system_print_dialog_data().FindBlob(
          kLinuxSystemPrintDialogDataPrintSettingsBin);
  if (settings_data) {
    if (auto dict = DeserializeValue<dbus_xdg::Dictionary>(*settings_data)) {
      ParseSettingsFromDictionary(*dict, settings_.get());
    }
  }

  const std::vector<uint8_t>* page_setup_data =
      settings.system_print_dialog_data().FindBlob(
          kLinuxSystemPrintDialogDataPageSetupBin);
  if (page_setup_data) {
    if (auto dict = DeserializeValue<dbus_xdg::Dictionary>(*page_setup_data)) {
      ParsePageSetupFromDictionary(*dict, settings_.get());
    }
  }

  const std::string* token_str = settings.system_print_dialog_data().FindString(
      kLinuxSystemPrintDialogDataPrintToken);
  if (token_str) {
    unsigned int token_value = 0;
    if (base::StringToUint(*token_str, &token_value)) {
      token_ = token_value;
    }
  }

  const std::string* parent_handle_str =
      settings.system_print_dialog_data().FindString(
          kLinuxSystemPrintDialogDataParentHandle);
  if (parent_handle_str) {
    parent_handle_ = *parent_handle_str;
  }
}
#endif

void PrintDialogLinuxPortal::ShowDialog(
    gfx::NativeView parent_view,
    bool has_selection,
    PrintingContextLinux::PrintSettingsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  parent_view_ = parent_view;
  callback_ = std::move(callback);

  dbus_xdg::RequestXdgDesktopPortal(
      bus_.get(), base::BindOnce(&PrintDialogLinuxPortal::OnPortalAvailable,
                                 weak_factory_.GetWeakPtr(), has_selection));
}

void PrintDialogLinuxPortal::OnPortalAvailable(bool has_selection,
                                               uint32_t version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (version < kXdgPortalRequiredVersion) {
    UseFallback(has_selection);
    return;
  }

  // Export the window handle to the portal.
  if (auto* delegate = ui::LinuxUiDelegate::GetInstance()) {
    aura::Window* window = parent_view_ ? parent_view_ : nullptr;
    if (window && window->GetRootWindow()) {
      // Assuming aura::Window* as NativeView.
      delegate->ExportWindowHandle(
          window->GetHost()->GetAcceleratedWidget(),
          base::BindOnce(&PrintDialogLinuxPortal::OnWindowHandleExported,
                         weak_factory_.GetWeakPtr(), has_selection));
      return;
    }
  }

  // No window or no delegate, call with empty handle.
  OnWindowHandleExported(has_selection, "");
}

void PrintDialogLinuxPortal::OnWindowHandleExported(bool has_selection,
                                                    std::string handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  parent_handle_ = std::move(handle);

  dbus::ObjectProxy* portal_proxy = bus_->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  dbus_xdg::Dictionary options;
  options[kKeyModal] = dbus_utils::Variant::Wrap<"b">(true);
  if (has_selection) {
    options["has_selected_pages"] = dbus_utils::Variant::Wrap<"b">(true);
  }

  // Prepare settings dictionary.
  dbus_xdg::Dictionary settings_dict;
  dbus_xdg::Dictionary page_setup_dict;
  if (settings_) {
    settings_dict = ConvertSettingsToDictionary(*settings_);
    page_setup_dict = ConvertPageSetupToDictionary(*settings_);
  }

  std::string title = l10n_util::GetStringUTF8(IDS_PRINT);

  // PreparePrint(parent_window, title, settings, page_setup, options)
  request_ = std::make_unique<dbus_xdg::Request>(
      bus_, portal_proxy, kPrintInterfaceName, kMethodPreparePrint,
      std::move(options),
      base::BindOnce(&PrintDialogLinuxPortal::OnPreparePrintResponse,
                     weak_factory_.GetWeakPtr()),
      parent_handle_, title, std::move(settings_dict),
      std::move(page_setup_dict));
}

void PrintDialogLinuxPortal::OnPreparePrintResponse(dbus_xdg::Results results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_.reset();

  if (!results.has_value()) {
    std::move(callback_).Run(mojom::ResultCode::kCanceled);
    return;
  }

  // Save the token for the subsequent Print call.
  if (auto it = results->find(kKeyToken); it != results->end()) {
    if (auto token = std::move(it->second).Take<uint32_t>()) {
      token_ = *token;
    }
  }

  // Ensure settings_ exists.
  if (!settings_) {
    settings_ = std::make_unique<PrintSettings>();
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  base::DictValue dialog_data;
#endif

  if (auto it = results->find(kKeySettings); it != results->end()) {
    if (auto settings_dict =
            std::move(it->second).Take<dbus_xdg::Dictionary>()) {
#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
      dialog_data.Set(kLinuxSystemPrintDialogDataPrintSettingsBin,
                      SerializeValue(*settings_dict));
#endif

      ParseSettingsFromDictionary(*settings_dict, settings_.get());
    }
  }

  // Ensure valid DPI before processing page setup.
  if (settings_->dpi() <= 0) {
    settings_->set_dpi(kDefaultPdfDpi);
  }

  if (auto it = results->find(kKeyPageSetup); it != results->end()) {
    if (auto page_setup_dict =
            std::move(it->second).Take<dbus_xdg::Dictionary>()) {
#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
      dialog_data.Set(kLinuxSystemPrintDialogDataPageSetupBin,
                      SerializeValue(*page_setup_dict));
#endif

      ParsePageSetupFromDictionary(*page_setup_dict, settings_.get());
    }
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  if (token_) {
    dialog_data.Set(kLinuxSystemPrintDialogDataPrintToken,
                    base::NumberToString(*token_));
  }
  dialog_data.Set(kLinuxSystemPrintDialogDataParentHandle, parent_handle_);
  settings_->set_system_print_dialog_data(std::move(dialog_data));

  // A dummy device name is required.
  settings_->set_device_name(kPrintDialogLinuxPortalDeviceName);
#endif

  // Update context with the settings selected by the user (or confirmed).
  if (context_) {
    context_->InitWithSettings(std::move(settings_));
  }

  std::move(callback_).Run(mojom::ResultCode::kSuccess);
}

void PrintDialogLinuxPortal::PrintDocument(
    const MetafilePlayer& metafile,
    const std::u16string& document_name) {
  // NB: This may be called from a different sequence.
  if (fallback_dialog_) {
    fallback_dialog_->PrintDocument(metafile, document_name);
    return;
  }

  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  if (!base::CreatePipe(&read_fd, &write_fd)) {
    LOG(ERROR) << "Failed to create pipe for printing";
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EnsureDBusInitialized, base::UTF16ToUTF8(document_name),
                     std::move(read_fd), bus_, parent_handle_,
                     std::move(token_)));

  std::vector<char> data;
  if (metafile.GetDataAsVector(&data)) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&WriteDataToPipe, std::move(write_fd), std::move(data)));
  } else {
    LOG(ERROR) << "Failed to get data from metafile";
  }
}

void PrintDialogLinuxPortal::UseFallback(bool has_selection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto* linux_ui = ui::LinuxUi::instance()) {
    fallback_dialog_ = linux_ui->CreatePrintDialog(context_);
  }

  if (!fallback_dialog_) {
    // Should not happen if LinuxUi is working, but handle gracefully.
    std::move(callback_).Run(mojom::ResultCode::kFailed);
    return;
  }

  if (settings_) {
    fallback_dialog_->UpdateSettings(std::move(settings_));
  } else {
    fallback_dialog_->UseDefaultSettings();
  }

  fallback_dialog_->ShowDialog(parent_view_, has_selection,
                               std::move(callback_));
}

}  // namespace printing
