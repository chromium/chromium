#include "components/attribution_reporting/pam_epsilon.h"

#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

// using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::TriggerRegistrationError;

constexpr char kPamEpsilon[] = "pam_epsilon";

double g_max_pam_epsilon = 14;

bool IsPamEpsilonValid(double epsilon) {
  return epsilon >= 0 && epsilon <= g_max_pam_epsilon;
}

}  // namespace

// static
base::expected<PamEpsilon, TriggerRegistrationError>
PamEpsilon::Parse(const base::Value::Dict& dict) {
  const base::Value* value = dict.Find(kPamEpsilon);
  if (!value) {
    return PamEpsilon();
  }

  std::optional<double> d = value->GetIfDouble();
  if (!d.has_value()) {
    return base::unexpected(
        TriggerRegistrationError::kPamEpsilonWrongType);
  }

  if (!IsPamEpsilonValid(*d)) {
    return base::unexpected(
        TriggerRegistrationError::kPamEpsilonValueInvalid);
  }

  return PamEpsilon(*d);
}

PamEpsilon::PamEpsilon()
    : PamEpsilon(g_max_pam_epsilon) {}

PamEpsilon::PamEpsilon(double epsilon) : epsilon_(epsilon) {
  CHECK(IsPamEpsilonValid(epsilon_));
}

bool PamEpsilon::SetIfValid(double epsilon) {
  if (!IsPamEpsilonValid(epsilon)) {
    return false;
  }
  epsilon_ = epsilon;
  return true;
}

void PamEpsilon::Serialize(base::Value::Dict& dict) const {
  dict.Set(kPamEpsilon, epsilon_);
}

ScopedMaxPamEpsilonForTesting::ScopedMaxPamEpsilonForTesting(
    double epsilon)
    : previous_(g_max_pam_epsilon) {
  CHECK_GE(epsilon, 0);
  g_max_pam_epsilon = epsilon;
}

ScopedMaxPamEpsilonForTesting::~ScopedMaxPamEpsilonForTesting() {
  g_max_pam_epsilon = previous_;
}

}  // namespace attribution_reporting
