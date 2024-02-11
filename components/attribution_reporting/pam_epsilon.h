#ifndef COMPONENTS_ATTRIBUTION_REPORTING_PAM_EPSILON_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_PAM_EPSILON_H_

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

// Controls the epsilon parameter requested to be consumed by the user
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) PamEpsilon {
 public:
  static base::expected<PamEpsilon, mojom::TriggerRegistrationError>
  Parse(const base::Value::Dict&);

  // Creates an epsilon with the maximum allowed value.
  PamEpsilon();

  // `CHECK()`s that the given value is non-negative and less than the maximum.
  explicit PamEpsilon(double);

  ~PamEpsilon() = default;

  PamEpsilon(const PamEpsilon&) = default;
  PamEpsilon& operator=(const PamEpsilon&) = default;

  PamEpsilon(PamEpsilon&&) = default;
  PamEpsilon& operator=(PamEpsilon&&) = default;

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
    ScopedMaxPamEpsilonForTesting {
 public:
  explicit ScopedMaxPamEpsilonForTesting(double);

  ~ScopedMaxPamEpsilonForTesting();

  ScopedMaxPamEpsilonForTesting(
      const ScopedMaxPamEpsilonForTesting&) = delete;
  ScopedMaxPamEpsilonForTesting& operator=(
      const ScopedMaxPamEpsilonForTesting&) = delete;

  ScopedMaxPamEpsilonForTesting(ScopedMaxPamEpsilonForTesting&&) =
      delete;
  ScopedMaxPamEpsilonForTesting& operator=(
      ScopedMaxPamEpsilonForTesting&&) = delete;

 private:
  double previous_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_PAM_EPSILON_H_
