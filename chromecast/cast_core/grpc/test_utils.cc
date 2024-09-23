#include "chromecast/cast_core/grpc/test_utils.h"

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/time/time.h"

namespace cast {
namespace test {

void StopGrpcServer(utils::GrpcServer& server, const base::TimeDelta& timeout) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  server.Stop(timeout.InMilliseconds(), run_loop.QuitClosure());
  run_loop.Run();
}

bool WaitForPredicate(const base::TimeDelta& timeout,
                      base::RepeatingCallback<bool()> predicate) {
  static constexpr auto kSleepTimeout = base::Milliseconds(10);
  for (int i = 0; i < timeout / kSleepTimeout; ++i) {
    if (predicate.Run()) {
      return true;
    }
    usleep(kSleepTimeout.InMilliseconds());
  }
  return false;
}

}  // namespace test
}  // namespace cast
