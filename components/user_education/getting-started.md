# Getting started with User Education

User Education is currently supported on Desktop Chrome and ChromeOS, with
support for other platforms in an exploratory phase. This site will mainly
concern itself with the Desktop Chrome implementation; please contact the
ChromeOS User Education developers for Ash-specific user education journeys.

Before you begin, ensure that you have an appropriate design for your journey
in your PRD which complies with the User Education Team's guidelines. Your UX
and PM should have consulted the
[Desktop User Education Homepage](https://sites.google.com/corp/google.com/desktopusereducation/home)
to design this User Education journey. Once you have a specification, you can
proceed with implementation.

There are four different types of desktop user education journeys. Consult the
link for the type(s) of journey your feature will use:
* [Feature Promos, also known as in-product help (IPH)](feature-promos.md)
* [Tutorials](tutorials.md)
* ["New" Badge](https://goto.google.com/new-badge-how-to)
* ["What's New" Page](https://sites.google.com/corp/google.com/desktopusereducation/implement/whats-new-page)

## Help Bubbles

IPH and Tutorials use the [Help Bubble](help-bubbles.md) system. This system
attaches help bubbles to named elements in the UI. If the element you wish to
attach a bubble to is in a WebUI page or WebUI-based secondary UI, you will
need to do additional work to prepare the page to support help bubbles;
[see here](./webui/README.md) for instructions.

# Adding User Education to your non-Chrome-Desktop application

Please contact [Frizzle Team](mailto:frizzle-team@google.com) for guidance on
how to use User Education on platforms outside Desktop Chrome.

# Adding help bubbles to new presentation frameworks

Please contact [Frizzle Team](mailto:frizzle-team@google.com) for guidance on
how to extend the Help Bubble system to new presentation frameworks beyond Views
and WebUI.
