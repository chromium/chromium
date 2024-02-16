#ifndef COMPONENTS_ATTRIBUTION_REPORTING_EPOCH_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_EPOCH_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) Epoch {
 public:
  static std::optional<Epoch> Create(uint64_t epoch_start, uint64_t epoch_end);

  static base::expected<Epoch, mojom::TriggerRegistrationError>
  FromJSON(base::Value& value);

  Epoch();

  ~Epoch();

  Epoch(const Epoch&);
  Epoch& operator=(const Epoch&);

  Epoch(Epoch&&);
  Epoch& operator=(Epoch&&);

  uint64_t epoch_start() const { return epoch_start_; }
  uint64_t epoch_end() const { return epoch_end_; }

  base::Value::Dict ToJson() const;

  friend bool operator==(const Epoch&, const Epoch&) = default;

 private:
  Epoch(uint64_t epoch_start, uint64_t epoch_end);

  uint64_t epoch_start_ = 0;
  uint64_t epoch_end_ = 0;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_EPOCH_H_
