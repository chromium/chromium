# Commander overview

This directory contains the bulk of the Commander, a new UI surface that allows
users to access most Chromium functionality via a keyboard interface.

## Components

The three main components of the commander are:

### Frontend

The frontend is responsible for passing user input to the controller and
displaying the subsequent results, delivered in a `CommanderViewModel`.
The frontend conforms to the `CommanderFrontend` interface which currently
has a single production implementation: `CommanderFrontendViews`. The actual
interface is implemented in WebUI (see `chrome/browser/ui/webui/commander`).

### Command Sources

Command sources (`CommandSource`) are responsible for providing a list of
possible commands (`CommandItem`) for the user's current context, then
executing them if they are chosen.

### Backend

The backend is responsible for maintaining alist of command sources, passing
user input to them, and sorting and ranking the result.

If a command requires more input (for example, choosing a window to move the
current tab to), the command source will provide a callback which can receive
user input and returns commands. The backend will then use this callback
as the sole source to complete the multi-step command.

The backend conforms to the `CommanderBackend` interface and currently has a
single production implementation: `CommanderController`.

## Relationships

To sum up the relationships of the classes in this file:
`Commander` (a singleton) owns a `CommanderBackend` and a `CommanderFrontend`.
This class's `ToggleForBrowser()` method is the entirety of the Commander
interface as far as the rest of the browser is concerned.

`ToggleForBrowser()` calls the method of the same name on the frontend. This
hides the UI if it is showing on a different browser, then hides or shows it on
the provided browser, depending on the current state. The frontend then sends
user input to the backend, which responds with a view model of possible
commands.

When the backend receives a message indicating that the user has chosen an
option, the `CommandItem` is executed if it's "one-shot". Otherwise, it's
a composite command; the backend passes a prompt text to the frontend, which
updates the UI accordingly. The backend passes subsequent input to the provided
callback until the command is either completed or cancelled. Composite commands
can be nested to allow for multiple (for example: the user specifying two
windows to merge together).

## Fuzzy finder
Most `CommandSource`s use `FuzzyFinder` to narrow down which commands to return.
For more information, see [the explainer](fuzzy_finder.md).
