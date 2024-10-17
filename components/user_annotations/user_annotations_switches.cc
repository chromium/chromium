#include "components/user_annotations/user_annotations_switches.h"

#include <optional>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

namespace user_annotations {
namespace switches {

// Overrides providing form annotations for seeding the annotation store.
// The value should be a base64 encoded of an annotations response.
const char kFormsAnnotationsOverride[] =
    "user-annotations-forms-annotation-override";

std::optional<optimization_guide::proto::FormsAnnotationsResponse>
ParseFormsAnnotationsFromCommandLine() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kFormsAnnotationsOverride)) {
    return std::nullopt;
  }

  std::string b64_pb = cmd_line->GetSwitchValueASCII(kFormsAnnotationsOverride);

  std::string binary_pb;
  if (!base::Base64Decode(b64_pb, &binary_pb)) {
    LOG(ERROR) << "Invalid base64 encoding of the Hints Proto Override";
    return std::nullopt;
  }

  optimization_guide::proto::FormsAnnotationsResponse forms_annotations =
      optimization_guide::proto::FormsAnnotationsResponse();

  if (!forms_annotations.ParseFromString(binary_pb)) {
    LOG(ERROR) << "Invalid proto provided to the Forms Annotation Override";
    return std::nullopt;
  }

  return forms_annotations;
}

}  // namespace switches
}  // namespace user_annotations
