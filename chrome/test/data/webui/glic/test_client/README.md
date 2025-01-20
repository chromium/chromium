# Glic test client

This is a test page that facilitates testing the API between the host and the web client.


## Instructions

1. Build it

```bash
   autoninja -C out/Default chrome/test/data/webui/glic:generate_test_files
```

2. Serve it

```bash
   python3 -m http.server -d out/Default/gen/chrome/test/data/webui/glic
```

3. Run Chrome with `--glic-guest-url=http://localhost:8000/glic/test_client/index.html`
