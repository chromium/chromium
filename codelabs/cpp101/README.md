# C++ in Chromium 101 - Codelab

This tutorial will guide you through the creation of various example C++
applications, highlighting important Chromium C++ concepts.
This tutorial assumes robust knowledge of C++ (the language) but does not
assume you know how to write an application specific to Chromium's style and
architecture. This tutorial does assume that you know how to check files out
of Chromium's repository.

As always, consider the following resources as of primary importance:

-   [Coding Style](https://chromium.googlesource.com/chromium/src/+/main/styleguide/styleguide.md)
-   [Callback<> and Bind()](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/callback.md)
-   [Threading and Tasks in Chrome](https://chromium.googlesource.com/chromium/src/+/main/docs/threading_and_tasks.md)
-   [Intro to Mojo & Services](https://chromium.googlesource.com/chromium/src.git/+/main/docs/mojo_and_services.md)
-   [Important Abstractions and Data Structures](https://sites.google.com/a/chromium.org/dev/developers/coding-style/important-abstractions-and-data-structures) (badly needs updating)

This tutorial does not assume you have read any of the above,
though you should feel free to peruse them when necessary.
This tutorial will cover information across all of those guides.

Exercise solutions are available in the [codelabs/cpp101/solutions](
https://source.chromium.org/chromium/chromium/src/+/main:codelabs/cpp101/solutions)
directory of the Chromium source code. Build all of the example solutions with
`autoninja -C out/Default codelab_cpp101`. You are encouraged to implement these
exercises yourself in the `codelabs/cpp101` directory.

### Prerequisite: Getting the Code

Before you can do the exercises you need to set up a system to checkout, build,
and run the code. Instructions can be found [here](https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code/).

### Exercise 0: "Hello World!"

This exercise demonstrates the use of the [ninja](https://ninja-build.org/)
build system to build a simple C++ binary and demonstrates how typical C++
builds are organized within Chromium.

Create a new target in `codelabs/cpp101/BUILD.gn` for a new executable
named `codelab_cpp101_hello_world`. Then write the classic "Hello, world!" program in
C++. You should be able to build it with
`autoninja -C out/Default codelab_cpp101_hello_world` and execute it directly by
finding the binary within `out/Default`.

Sample execution:
```shell
$ cd /path/to/chromium/src
$ gclient runhooks
$ autoninja -C out/Default codelab_cpp101_hello_world
$ out/Default/codelab_cpp101_hello_world
Hello, world!
[0923/185218.645640:INFO:hello_world.cc(27)] Hello, world!
```

### More information
[Targets](https://gn.googlesource.com/gn/+/refs/heads/main/docs/language.md#Targets)

[Git Tips](https://chromium.googlesource.com/chromium/src.git/+/main/docs/git_tips.md)
and [Git Cookbook](https://chromium.googlesource.com/chromium/src.git/+/main/docs/git_cookbook.md)

[Life of a Chromium Developer](https://docs.google.com/a/google.com/presentation/d/1abnqM9j6zFodPHA38JG1061rG2iGj_GABxEDgZsdbJg/)

## Part 1: Using command-line arguments

We will augment our `codelab_cpp101_hello_world` binary to parse command-line flags and
use those values to print messages to the user.

Command-line arguments within Chromium are processed by the
`CommandLine::Init()` function, which takes command line flags from the
[argc and argv](https://crasseux.com/books/ctutorial/argc-and-argv.html)
(argument count & vector) variables of the main() method. A typical invocation
of `CommandLine::Init()` looks like the following:
```cpp
int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  // Main program execution ...
  return 0;
}
```
Flags are not explicitly defined in Chromium. Instead, we
use `GetSwitchValueASCII()` and friends to retrieve values passed in.

### Important include files

```cpp
#include "base/command_line.h"
#include "base/logging.h"
```

### Exercise 1: Using command-line arguments

Change `codelab_cpp101_hello_world` to take a `--greeting` and a `--name` switch.
The greeting, if not specified, should default to "Hello",
and the name, if not specified, should default to "World".

## Part 2: Callbacks and Bind

C++, unlike other languages such as Python, Javascript, or Lisp, has only
rudimentary support for [callbacks](https://en.wikipedia.org/wiki/Callbacks)
and no support for
[partial application](https://en.wikipedia.org/wiki/Partial_application).
However, Chromium has the `base::OnceCallback<Sig>` and
 `base::RepeatingCallback<Sig>`class, whose instances can be freely passed
around, returned, and generally be treated as first-class values.
base::OnceCallback<Sig> is the move-only, single-call variant,
and base::RepeatingCallback<Sig> is the copyable, multiple-call variant.

The `Sig` template parameter is a function signature type:
```cpp
// The type of a callback that:
//  - Can run only once.
//  - Is move-only and non-copyable.
//  - Takes no arguments and does not return anything.
// base::OnceClosure is an alias of this type.
base::OnceCallback<void()>

// The type of a callback that:
//  - Can run more than once.
//  - Is copyable.
//  - Takes no arguments and does not return anything.
// base::RepeatingClosure is an alias of this type.
base::RepeatingCallback<void()>

// The types of a callback that takes two arguments (a string and a double)
// and returns an int.
base::OnceCallback<int(std::string, double)>
base::RepeatingCallback<int(std::string, double)>
```
Callbacks are executed by invoking the `Run()` member function.
base::OnceCallback<Sig> needs to be rvalue to run.
```cpp
void MyFunction1(base::OnceCallback<int(std::string, double)> my_callback) {
  // OnceCallback
  int result1 = std::move(my_callback).Run("my string 1", 1.0);

  // After running a OnceCallback, it's consumed and nulled out.
  DCHECK(!my_callback);
  ...
}

void MyFunction2(base::RepeatingCallback<int(std::string, double)> my_callback) {
  int result1 = my_callback.Run("my string 1", 1.0);
  // Run() can be called as many times as you wish for RepeatingCallback.
  int result2 = my_callback.Run("my string 2", 2);
  ...
```
Callbacks are constructed using the `base::BindOnce()` or `base::BindRepeating()` function,
which handles partial application:
```cpp
// Declare a function.
void MyFunction(int32 a, double b);

base::OnceCallback<void(double)> my_callback1 = base::BindOnce(&MyFunction, 10);
base::RepeatingCallback<void(double)> my_callback2 = base::BindRepeating(&MyFunction, 10);

// Equivalent to:
//
// MyFunction(10, 3.5);
//
std::move(my_callback1).Run(3.5);
my_callback2.Run(3.5);
```
`base::BindOnce()` and `base::BindRepeating()` can do a lot more, including
binding class member functions and binding additional arguments to an
existing `base::OnceCallback` or `base::RepeatingCallback`. See
[docs/callback.md](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/callback.md)
for details.

### Important Include Files

```cpp
#include "base/functional/bind.h"
#include "base/functional/callback.h"
```

### More Information

[Callback<> and Bind()](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/callback.md)

### Exercise 2: Fibonacci closures

Implement a function that returns a callback that takes no arguments and returns
successive Fibonacci numbers. That is, a function that can be used like this:
```cpp
base::RepeatingCallback<int()> fibonacci_closure = MakeFibonacciClosure();
LOG(INFO) << fibonacci_closure.Run(); // Prints "1"
LOG(INFO) << fibonacci_closure.Run(); // Prints "1"
LOG(INFO) << fibonacci_closure.Run(); // Prints "2"
...
```
Each returned Fibonacci callback should be independent;
running one callback shouldn't affect the result of running another callback.
Write a `fibonacci` executable that takes an integer argument `n`
and uses your function to print out the first `n` Fibonacci numbers.

(This exercise was inspired by
[this Go exercise: Function closures](https://tour.golang.org/moretypes/25).)

## Part 3: Threads and task runners

Chromium has a number of abstractions for sequencing and threading.
[Threading and Tasks in Chrome](https://chromium.googlesource.com/chromium/src/+/main/docs/threading_and_tasks.md)
is a must-read and go-to reference for anything related to tasks, thread pools,
task runners, and more.

Sequenced execution (on virtual threads) is strongly preferred to
single-threaded execution (on physical threads). Chromium's abstraction for
asynchronously running posted tasks is `base::TaskRunner`. Task runners allow
you to write code that posts tasks without depending on what exactly will run
those tasks.

`base::SequencedTaskRunner` (which extends `base::TaskRunner`) is a commonly
used abstraction which handles running tasks (which are instances
of `base::OnceClosure`) in sequential order. These tasks are not guaranteed to
run on the same thread. The preferred way of posting to the current (virtual)
thread is `base::SequencedTaskRunner::GetCurrentDefault()`.

A task that can run on any thread and doesn’t have ordering or mutual exclusion
requirements with other tasks should be posted using one of the
`base::ThreadPool::PostTask()` functions.

There are a number of ways to post tasks to a thread pool or task runner.

- `PostTask()`
- `PostDelayedTask()` if you want to add a delay.
- `PostTaskAndReply()` lets you post a task which will post a task back to your
  current thread when its done.
- `PostTaskAndReplyWithResult()` to automatically pass the return value of the
  first call as argument to the second call.

Normally you wouldn't have to worry about setting up a threading environment and
keeping it running, since that is automatically done by Chromium's thread
classes. However, since the main thread doesn't automatically start off with
`TaskEnvironment`, there's a bit of extra setup involved. The following setup
code should be enough to create the necessary TaskEnvironment.
Include `testonly=true` flag in the BUILD.gn file, along with
`"//base/test:test_support"` set as a dependency.

### Important header files
```cpp
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/at_exit.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/command_line.h"
```
### Setup code:
```cpp
int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  TestTimeouts::Initialize();
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};

  // The rest of your code will go here.
```

### Exercise 3a: Sleep

Implement the Unix command-line utility `sleep` using only
a `base::SequencedTaskRunner::CurrentDefaultHandle` (i.e., without using the `sleep` function
or `base::PlatformThread::Sleep`).
Hint: You will need to use `base::RunLoop` to prevent the main function from
exiting prematurely.

### Exercise 3b: Integer factorization

Take the given (slow) function to find a non-trivial factor of a given integer:
```cpp
std::optional<int> FindNonTrivialFactor(int n) {
  // Really naive algorithm.
  for (int i = 2; i < n; ++i) {
    if (n % i == 0) {
      return i;
    }
  }
  return std::nullopt;
}
```
Write a command-line utility `factor` that takes a number, posts a task to the
background using `FindNonTrivialFactor`, and prints a status update every second
as long as the factoring task is executing.

### More information

[Threading and Tasks in Chrome](https://chromium.googlesource.com/chromium/src/+/main/docs/threading_and_tasks.md)

## Part 4: Mojo

Mojo is Chromium's abstraction of IPC. Mojo allows for developers to easily
connect interface clients and implementations across arbitrary intra- and
inter-process boundaries. See the
[Intro to Mojo and Services](https://chromium.googlesource.com/chromium/src.git/+/main/docs/mojo_and_services.md)
guide to get started.

 ### Exercise 4: Building a simple out-of-process service

See the [building a simple out-of-process service](https://chromium.googlesource.com/chromium/src.git/+/main/docs/mojo_and_services.md#example_building-a-simple-out_of_process-service)
tutorial on using Mojo to define, hook up, and launch an out-of-process service.

### More Information

[Mojo C++ Bindings API Docs](https://chromium.googlesource.com/chromium/src.git/+/main/mojo/public/cpp/bindings/README.md)
[Mojo Docs](https://chromium.googlesource.com/chromium/src.git/+/main/mojo/README.md)
