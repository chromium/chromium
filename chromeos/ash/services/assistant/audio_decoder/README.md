This directory contains audio decoder service for the Chrome OS native Assistant
to decode the audio output by Libassistant, before connecting to AudioService.
We cannot use the standard media service, which does not have the demuxer.
