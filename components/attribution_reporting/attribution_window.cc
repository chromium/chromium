#include "components/attribution_reporting/attribution_window.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

constexpr char kEpochStart[] = "epoch_start";
constexpr char kEpochEnd[] = "epoch_end";

bool IsAttributionWindowValid(const uint64_t epoch_start, uint64_t epoch_end) {
  return epoch_start <= epoch_end && epoch_start >=0 && epoch_end >=0;
}

base::expected<uint64_t, TriggerRegistrationError> ParseAttributionWindowStart(
    const base::Value::Dict& registration) {
  const base::Value* v = registration.Find(kEpochStart);
  if (!v) {
    return base::unexpected(
        TriggerRegistrationError::kAttributionWindowStartMissing);
  }
  if (std::optional<uint64_t> epoch_start = v->GetIfInt()) {
    if (*epoch_start < 0) {
      return base::unexpected(TriggerRegistrationError::kAttributionWindowValueInvalid);
    }
    return *epoch_start;
  }
    return base::unexpected(
        TriggerRegistrationError::kAttributionWindowStartMissing);
}

base::expected<uint64_t, TriggerRegistrationError> ParseAttributionWindowEnd(
    const base::Value::Dict& registration) {
  const base::Value* v = registration.Find(kEpochEnd);
  if (!v) {
    return base::unexpected(
        TriggerRegistrationError::kAttributionWindowEndMissing);
  }
  if (std::optional<uint64_t> epoch_end = v->GetIfInt()) {
    if (*epoch_end < 0) {
      return base::unexpected(TriggerRegistrationError::kAttributionWindowValueInvalid);
    }
    return *epoch_end;
  }
    return base::unexpected(
        TriggerRegistrationError::kAttributionWindowEndMissing);
}


}  // namespace

// static
std::optional<AttributionWindow> AttributionWindow::Create(uint64_t epoch_start, uint64_t epoch_end) {
  if (!IsAttributionWindowValid(epoch_start, epoch_end))
    return std::nullopt;
  return AttributionWindow(epoch_start, epoch_end);
}

// static
base::expected<AttributionWindow, TriggerRegistrationError>
AttributionWindow::FromJSON(const base::Value* value) {
  const base::Value::Dict* dict = value->GetIfDict();
  if (!dict) {
    return base::unexpected(
        TriggerRegistrationError::kAttributionWindowWrongType);
  }

  ASSIGN_OR_RETURN(auto epoch_start, ParseAttributionWindowStart(*dict));
  ASSIGN_OR_RETURN(auto epoch_end, ParseAttributionWindowEnd(*dict));
  return AttributionWindow(epoch_start, epoch_end);
}

AttributionWindow::AttributionWindow() = default;

AttributionWindow::AttributionWindow(uint64_t epoch_start, uint64_t epoch_end)
    : epoch_start_(epoch_start), epoch_end_(epoch_end) {
  DCHECK(IsAttributionWindowValid(epoch_start_, epoch_end_));
}

AttributionWindow::~AttributionWindow() = default;

AttributionWindow::AttributionWindow(
    const AttributionWindow&) = default;

AttributionWindow& AttributionWindow::operator=(
    const AttributionWindow&) = default;

AttributionWindow::AttributionWindow(AttributionWindow&&) =
    default;

AttributionWindow& AttributionWindow::operator=(
    AttributionWindow&&) = default;

base::Value::Dict AttributionWindow::ToJson() const {
  base::Value::Dict dict;

  SerializeUint64(dict, kEpochStart, epoch_start_);
  SerializeUint64(dict, kEpochEnd, epoch_end_);
  return dict;
}

}  // namespace attribution_reporting
