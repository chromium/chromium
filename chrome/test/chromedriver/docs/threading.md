# Threading

ChromeDriver uses 3 types of threads:

* One command thread. This is the primary thread that executes 
  the `main` function and starts the ChromeDriver process.\
  It is also responsible for parsing each incoming command to determine its
  target session and dispatch it accordingly.

* One I/O thread. Responsible for I/O with clients and with Chrome.

* Session threads, one for each session. It is responsible for executing all
  ChromeDriver commands that target a specific session.

## Thread Interactions

As one would expect, threads cannot directly call each other.
Instead, ChromeDriver uses mechanisms provided by Chromium base library
to post tasks between threads.

Each thread in ChromeDriver is wrapped by a
[`base::Thread`](https://source.chromium.org/chromium/chromium/src/+/main:base/threading/thread.h?q=%22class%20BASE_EXPORT%20Thread%22) object.
This object exposes a
[`base::TaskRunner`](https://source.chromium.org/chromium/chromium/src/+/main:base/task/task_runner.h?q=base::TaskRunner),
which provides the ability to post tasks to that thread.\
When thread A wants to call a function on thread B, it finds the `TaskRunner`
object corresponding to thread B, and calls its `TaskRunner::PostTask` method.\
This will cause a task to be posted to thread B. When thread B is not busy,
it will execute tasks posted to it, in the order they were queued.

A task cannot directly return results to the calling thread.
If a response is desired, thread B can post a task back to thread A.

## Conventions

Functions intended as inter-thread tasks have special names to make
it clear which threads they should run on.
* A function intended for the command thread should have name ending with
  `OnCmdThread` or `OnCommandThread`, e.g., `HandleRequestOnCmdThread`.
* A function intended for the I/O thread has name ending with `OnIOThread`,
  e.g., `StartServerOnIOThread`.
* A function for a session thread has name ending with `OnSessionThread`,
  e.g., `ExecuteSessionCommandOnSessionThread`.

## Scenarios

This section details thread transitions in several scenarios.

### Process Initialization

The [`main`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/server/chromedriver_server.cc?q=%22int%20main%22)
function runs on the command thread.
After some initialization, it calls `RunServer`.

[`RunServer`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/server/chromedriver_server.cc?q=RunServer)
also runs on the command thread. It does the following:
* Creates the I/O thread.
* Posts a task to run `StartServerOnIOThread` on the I/O thread.
* Run an event loop, and waits for incoming tasks posted from other threads.

The final part of initialization occurs in
[`StartServerOnIOThread`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/server/chromedriver_server.cc?q=StartServerOnIOThread)
on the I/O thread.
It creates an [`HttpServer`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/server/http_server.h?q=%22class%20HttpServer%22),
and then calls `HttpServer::Start` to start listening for incoming WebDriver
requests.

### New Request from Client

When a new request is received from a client, two things happen on the I/O
thread.
* [`HttpServer::OnHttpRequest`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/server/http_server.cc?q=HttpServer::OnHttpRequest)
  runs `HandleRequestOnIOThread`,
  which is stored in `HttpServer::handle_request_func_`.
* [`HandleRequestOnIOThread`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/server/chromedriver_server.cc?q=%22void%20HandleRequestOnIOThread%22)
  posts a task to run `HandleRequestOnCmdThread`.

Then several activities happen on the command thread.
* [`HandleRequestOnCmdThread`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/server/chromedriver_server.cc?q=%22void%20HandleRequestOnCmdThread%22)
  verifies that the request comes from a trusted IP address.
* [`HttpHandler::Handle`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/server/http_handler.cc?q=HttpHandler::Handle)
  does some validation of the incoming request.
* [`HttpHandler::HandleCommand`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/server/http_handler.cc?q=HttpHandler::HandleCommand)
  matches the request against a list of commands implemented by ChromeDriver,
  and dispatches the request to the appropriate command handler.

If the command is global (i.e., affecting all sessions), it is executed directly
on the command thread. Most commands, however, target specific sessions,
and are dispatched to the appropriate session thread to execute.\
New session command (details below) does not target a specific session; instead it creates a new session thread for the session so that it can later be targeted by other commands. 

Regardless of which thread executes the command, when the command finishes,
[`HttpHandler::PrepareResponse`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/server/http_handler.cc?q="void%20HttpHandler::PrepareResponse")
runs on command thread to prepare the response, and then the I/O thread sends
the response back to the client.

### New Session

Before a client can do anything useful, it must first call the InitSession
command to create a new session.
This command is first dispatched to
[`ExecuteCreateSession`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/commands.cc?q=ExecuteCreateSession),
which runs on the command thread and does the following.
* Generates a new session ID, which is defined by
  [W3C WebDriver Standard](https://www.w3.org/TR/webdriver/#dfn-session-id)
  as a 128-bit random hexadecimal string.
  This session ID is returned to the client, and is used
  to identify this session in all future requests.
* Creates a new `Session` object to store session-specific data.
* Creates and initializes a session thread.
* Invokes [`ExecuteInitSession`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/session_commands.cc?q=ExecuteInitSession)
  on the newly created session thread.

Then the session thread is responsible for parsing the requested capabilities,
starting Chrome, and initializing the session.

### Dispatching Command to a Session

Most WebDriver protocol commands apply to a particular session. In such cases,
[`ExecuteSessionCommand`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/commands.cc?q="ExecuteSessionCommand%28")
is responsible for finding the target session thread and posting a task to it.
