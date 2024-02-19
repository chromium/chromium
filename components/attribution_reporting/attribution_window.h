#ifndef COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_WINDOW_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_WINDOW_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AttributionWindow {
 public:
  static std::optional<AttributionWindow> Create(uint64_t epoch_start, uint64_t epoch_end);

  static base::expected<AttributionWindow, mojom::TriggerRegistrationError>
  FromJSON(const base::Value* value);

  AttributionWindow();

  ~AttributionWindow();

  AttributionWindow(const AttributionWindow&);
  AttributionWindow& operator=(const AttributionWindow&);

  AttributionWindow(AttributionWindow&&);
  AttributionWindow& operator=(AttributionWindow&&);

  uint64_t epoch_start() const { return epoch_start_; }
  uint64_t epoch_end() const { return epoch_end_; }

  base::Value::Dict ToJson() const;

  friend bool operator==(const AttributionWindow&, const AttributionWindow&) = default;

 private:
  AttributionWindow(uint64_t epoch_start, uint64_t epoch_end);

  uint64_t epoch_start_ = 0;
  uint64_t epoch_end_ = 0;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_WINDOW_H_
