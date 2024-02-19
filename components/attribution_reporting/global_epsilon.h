#ifndef COMPONENTS_ATTRIBUTION_REPORTING_GLOBAL_EPSILON_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_GLOBAL_EPSILON_H_

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

// Controls the epsilon parameter requested to be consumed by the user
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) GlobalEpsilon {
 public:
  static base::expected<GlobalEpsilon, mojom::TriggerRegistrationError>
  Parse(const base::Value::Dict&);

  // Creates an epsilon with the maximum allowed value.
  GlobalEpsilon();

  // `CHECK()`s that the given value is non-negative and less than the maximum.
  explicit GlobalEpsilon(double);

  ~GlobalEpsilon() = default;

  GlobalEpsilon(const GlobalEpsilon&) = default;
  GlobalEpsilon& operator=(const GlobalEpsilon&) = default;

  GlobalEpsilon(GlobalEpsilon&&) = default;
  GlobalEpsilon& operator=(GlobalEpsilon&&) = default;

  // This implicit conversion is allowed to ease drop-in use of
  // this type in places currently requiring `int` with prior validation.
  operator double() const {  // NOLINT
    return epsilon_;
  }

  [[nodiscard]] bool SetIfValid(double);

  void Serialize(base::Value::Dict&) const;

 private:
  double epsilon_;
};

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
    ScopedMaxGlobalEpsilonForTesting {
 public:
  explicit ScopedMaxGlobalEpsilonForTesting(double);

  ~ScopedMaxGlobalEpsilonForTesting();

  ScopedMaxGlobalEpsilonForTesting(
      const ScopedMaxGlobalEpsilonForTesting&) = delete;
  ScopedMaxGlobalEpsilonForTesting& operator=(
      const ScopedMaxGlobalEpsilonForTesting&) = delete;

  ScopedMaxGlobalEpsilonForTesting(ScopedMaxGlobalEpsilonForTesting&&) =
      delete;
  ScopedMaxGlobalEpsilonForTesting& operator=(
      ScopedMaxGlobalEpsilonForTesting&&) = delete;

 private:
  double previous_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_GLOBAL_EPSILON_H_
