# Global Media Controls

The Global Media Controls use information from the [Media Session
service](/services/media_session) as well as other sources (e.g. Cast sessions)
to provide a set of controls for all currently active media items, so it is
layered on top of the Media Session service instead of being a part of it.
This component provides UI to its users in the form of [Views](/ui/views)
components.

This component is or will be used by both //chrome and //ash, so it cannot
directly depend on either or on //content (since //ash cannot depend on
//content).
