#include "chromecast/cast_core/grpc/test_utils.h"

#include "base/run_loop.h"
#include "base/time/time.h"

namespace cast {
namespace test {

void StopGrpcServer(utils::GrpcServer& server, const base::TimeDelta& timeout) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  server.Stop(timeout.InMilliseconds(), run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace test
}  // namespace cast
